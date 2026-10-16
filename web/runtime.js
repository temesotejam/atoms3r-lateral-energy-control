'use strict';
const $ = id => document.getElementById(id);
let previewRunning = false, previewEvidence = null;
let poseRunning = false, poseCancelled = false, poseSession = null, poseSaved = false;
let latest = null, lastSeen = 0, refreshInFlight = false, commandInFlight = false;
let transferRunning = false, cancelTransfer = false, completedFile = null, completedName = '', completedFoot = null;
let wheelWorkReport = null;
function calculateWheelWork() {
  wheelWorkReport = null;
  try {
    if (!completedFile) throw Error('先にRWLOGを取得してください。');
    wheelWorkReport = WheelWorkAnalyzer.fromBytes(completedFile, {
      inertia:Number($('work-inertia').value), sign:Number($('work-sign').value)});
    $('work-result').textContent = completedName + '\n' + WheelWorkAnalyzer.describe(wheelWorkReport);
  } catch (error) { $('work-result').textContent = error.message; }
}
function saveWheelWorkCsv(kind) {
  if (!wheelWorkReport) { $('work-result').textContent = '先に仕事を計算してください。'; return; }
  const csv = WheelWorkAnalyzer.csv(wheelWorkReport[kind], wheelWorkReport);
  saveBlob(new Blob(['\ufeff', csv], {type:'text/csv;charset=utf-8'}),
    completedName.replace(/\.rwlog$/, `_wheel_work_${kind}.csv`));
}
let offlineMode = false, offlineUntil = 0;
let offlineGeneration = 0;
const offlineKey = 'freefoot-offline-run-until';
const targetChoices = [8, 10, 12];
let targetInitialized = false, targetBootId = null, requestedTargetDeg = null;
let angleInitialized = false, angleBootId = null, requestedInputPercent = null;
let currentInitialized = false, currentBootId = null, requestedInputCurrentMa = null;
let brakeInitialized = false, brakeBootId = null, requestedBrakeMaxMs = null;
const brakeChoices = [0, 20, 50];
let adaptationInitialized = false, adaptationBootId = null, requestedAdaptation = null;
let modelInitialized = false, modelBootId = null, requestedEnergyModel = null;
const validInputCurrent = value => Number.isInteger(value) && value >= 100 && value <= 1200 && value % 10 === 0;
const validInputPercent = value => value === 0;
const nap = ms => new Promise(resolve => setTimeout(resolve, ms));
function crc32(data, crc = 0) {
  crc = ~crc;
  for (const b of data) {
    crc ^= b;
    for (let bit = 0; bit < 8; ++bit) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (~crc) >>> 0;
}
async function request(path, {method = 'GET', kind = 'json', timeout = 3000} = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeout);
  try {
    const response = await fetch(path, {method, signal: controller.signal, cache: 'no-store'});
    if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
    // Abort remains armed through body consumption, including response.json().
    if (kind === 'bytes') return new Uint8Array(await response.arrayBuffer());
    if (kind === 'text') return await response.text();
    return await response.json();
  } finally { clearTimeout(timer); }
}
function controls() {
  const fresh = !offlineMode && latest && Date.now() - lastSeen < 3500;
  const busy = commandInFlight || previewRunning || poseRunning || !!latest?.command?.pending;
  const building = latest?.export_phase === 'building';
  $('start').disabled = !fresh || busy || transferRunning || !latest.ready || !latest.upright_reference?.valid || latest.running || latest.export_phase !== 'empty';
  $('upright-reference-capture').disabled = !fresh || busy || transferRunning || building ||
    !latest.controller_fresh || !latest.imu_ok || latest.running || latest.pulse_active || latest.motor_mA !== 0 ||
    !['READY_TO_MEASURE', 'FINISHED', 'ESTOP'].includes(latest.state);
  $('target').disabled = $('start').disabled;
  $('input-percent').disabled = $('start').disabled;
  $('input-current').disabled = $('start').disabled;
  $('energy-model').disabled = $('start').disabled;
  $('brake-max').disabled = $('start').disabled || $('energy-model').value === 'bridge';
  $('peak-adaptation').disabled = $('start').disabled;
  $('clear').disabled = !fresh || busy || transferRunning || building || latest.running || !['FINISHED', 'ESTOP'].includes(latest.state);
  $('download').disabled = !fresh || busy || transferRunning || !latest.downloadable || latest.running;
  $('cancel').disabled = !transferRunning;
  $('csv').disabled = !completedFoot;
  $('preview').disabled = !fresh || busy || transferRunning || building || latest.running || !latest.foot?.preview_available;
  $('preview-save').disabled = !previewEvidence || previewRunning;
  const poseReady = fresh && latest.controller_fresh && !busy && !transferRunning && !building && !latest.running &&
    latest.state === 'READY_TO_MEASURE' && latest.foot?.zero_ready && latest.foot?.preview_available && latest.mekf?.inputs?.valid;
  $('pose-base').disabled = !poseReady || !!poseSession;
  $('pose-add').disabled = !poseReady || !poseSession?.baseline || poseSession.poses.length >= PoseComparison.limits.max_poses;
  $('pose-cancel').disabled = !poseRunning;
  $('pose-save').disabled = !poseSession?.baseline || poseRunning;
  $('pose-reset').disabled = !poseSession || poseRunning || (!!poseSession.baseline && !poseSaved);
  if ($('usb-diag')) $('usb-diag').disabled = !fresh || commandInFlight;
  if ($('reconnect')) $('reconnect').style.display = offlineMode ? 'inline-block' : 'none';
}
function setOffline(waitMs) {
  ++offlineGeneration;
  offlineMode = true; offlineUntil = Date.now() + Math.max(0, Math.min(30000, waitMs));
  try { sessionStorage.setItem(offlineKey, String(offlineUntil)); } catch (_) {}
  renderOffline();
}
function clearOffline() {
  ++offlineGeneration;
  offlineMode = false; offlineUntil = 0;
  try { sessionStorage.removeItem(offlineKey); } catch (_) {}
}
function renderOffline() {
  const remaining = Math.max(0, Math.ceil((offlineUntil - Date.now()) / 1000));
  $('connection').textContent = remaining ? 'Web更新を休止中 · Wi-Fi接続を維持してください' : 'Webの復帰待ち · 自動再試行';
  $('state').textContent = 'Web休止中';
  $('run-target').textContent = requestedTargetDeg === null
    ? '目標角度はWeb復帰後に確認します。'
    : `目標角度：${requestedTargetDeg}°（ZEROクロスで入力時間を計算）`;
  $('run-percent').textContent = requestedInputPercent === null
    ? '通常入力はZEROクロスで行います。'
    : `開始要求：ZEROクロスで${requestedInputCurrentMa} mA、入力時間はエネルギーから計算`;
  $('run-brake').textContent = requestedBrakeMaxMs === null
    ? '負側ブレーキ上限はWeb復帰後に確認します。'
    : `開始要求：負側ブレーキ上限 ${requestedBrakeMaxMs} ms`;
  $('run-model').textContent = requestedEnergyModel === 'bridge'
    ? '開始要求：旧到達角予測をエネルギーに換算（逆入力なし）'
    : requestedEnergyModel === 'quadratic' ? '開始要求：二次エネルギー式' : '予測式はWeb復帰後に確認します。';
  $('remaining').textContent = remaining ? `${remaining} s（Web復帰目安）` : '復帰待ち';
  $('run-adaptation').textContent = requestedAdaptation === null
    ? '学習補正の設定はWeb復帰後に確認します。'
    : `開始要求：予測誤差の学習補正 ${requestedAdaptation ? '使用する' : '使用しない'}`;
  for (const id of ['pitch', 'current', 'right', 'left', 'fps']) $(id).textContent = '—';
  $('guide').textContent = '本体で制御・観測・記録を行います。開始5秒＋測定15秒＋終了5秒が予定時間です。前後90°以上の傾斜でSTOPします。横倒しは姿勢STOPの対象にしません。表示時間はPC側の目安で、実際の進行・終了を確認した値ではありません。';
  $('foot-status').textContent = '足角度の画面更新を停止。本体内の記録は継続します。';
  $('mekf-axes').textContent = '運転中の姿勢表示を停止しています。';
  controls();
}
const format = (n, digits = 2) => Number.isFinite(n) ? n.toFixed(digits) : '—';
function drawMarkers(f) {
  const ctx = $('markers').getContext('2d'); ctx.clearRect(0, 0, 640, 120);
  for (const [name, x, y, range] of [['A / 右', f.right_x, 35, f.range?.right_support_x ?? [42, 173]],
      ['B / 左', f.left_x, 90, f.range?.left_support_x ?? [43.5, 177.5]]]) {
    const [lo, hi] = range;
    ctx.fillStyle = '#dce8e5'; ctx.fillRect(lo * 2, y - 13, (hi - lo) * 2, 26);
    ctx.strokeStyle = '#aab8c6'; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(640, y); ctx.stroke();
    ctx.fillStyle = '#304962'; ctx.font = '12px system-ui'; ctx.fillText(name, 5, y - 16);
    if (Number.isFinite(x)) { ctx.fillStyle = '#145fad'; ctx.beginPath(); ctx.arc(x * 2, y, 7, 0, 2 * Math.PI); ctx.fill(); }
  }
}
function footIssues(f, stale, terminal) {
  const issues = [];
  if (terminal) issues.push('最終フレームを表示');
  else if (stale) issues.push('画像更新なし');
  if (f.frame_valid === false) issues.push('画像取得失敗');
  else if (f.frame_timestamp_valid === false) issues.push('画像時刻が無効');
  const reasons = {low_contrast: '未検出（明暗差不足）', low_weight: '未検出（白領域不足）',
    bad_shape: '白領域の幅が不適合', ambiguous: '候補を区別できません',
    track_jump: '検出位置が急変', reacquiring: '再捕捉中'};
  for (const [side, label] of [['right', '右'], ['left', '左']]) {
    if (f[side + '_valid']) {
      if (f[side + '_in_range'] === false) issues.push(label + '：設定範囲外');
    } else {
      const reason = f[side + '_reason'];
      if (reasons[reason]) issues.push(label + '：' + reasons[reason]);
      else if (reason === 'no_frame' && f.frame_valid !== false) issues.push(label + '：画像なし');
      else if (!reason) issues.push(label + '：未検出または未校正'); // Older status data.
    }
  }
  if (f.overflow) issues.push('記録容量超過');
  return issues.length ? ' · ' + issues.join(' · ') : '';
}
function render(s) {
  $('connection').textContent = s.controller_fresh ? '接続中' : '接続中 · 制御状態の更新が停止';
  $('state').textContent = s.state; $('remaining').textContent = format(s.remaining_ms / 1000, 1) + ' s';
  $('pitch').textContent = format(s.pitch_deg) + '°';
  $('current').textContent = `${s.motor_mA} / ${s.actual_mA} mA`;
  const f = s.foot, stale = !Number.isFinite(f.age_ms) || f.age_ms < 0 || f.age_ms > 500;
  const terminal = ['FINISHED', 'ESTOP'].includes(s.state);
  if (targetChoices.includes(s.target_deg)) {
    if (!targetInitialized || targetBootId !== s.boot_id || s.running || terminal) {
      $('target').value = String(s.target_deg);
      targetInitialized = true; targetBootId = s.boot_id;
    }
  }
  if (validInputPercent(s.input_peak_percent) &&
      (!angleInitialized || angleBootId !== s.boot_id || s.running || terminal)) {
    $('input-percent').value = String(s.input_peak_percent);
    angleInitialized = true; angleBootId = s.boot_id;
  }
  if (validInputCurrent(s.input_current_mA) &&
      (!currentInitialized || currentBootId !== s.boot_id || s.running || terminal)) {
    $('input-current').value = String(s.input_current_mA);
    currentInitialized = true; currentBootId = s.boot_id;
  }
  if (brakeChoices.includes(s.brake_max_ms) &&
      (!brakeInitialized || brakeBootId !== s.boot_id || s.running || terminal)) {
    $('brake-max').value = String(s.brake_max_ms);
    brakeInitialized = true; brakeBootId = s.boot_id;
  }
  if (['bridge','quadratic'].includes(s.energy_model) &&
      (!modelInitialized || modelBootId !== s.boot_id || s.running || terminal)) {
    $('energy-model').value = s.energy_model;
    modelInitialized = true; modelBootId = s.boot_id;
  }
  if (!adaptationInitialized || adaptationBootId !== s.boot_id || s.running || terminal) {
    $('peak-adaptation').value = s.peak_adaptation_enabled ? '1' : '0';
    adaptationInitialized = true; adaptationBootId = s.boot_id;
  }
  $('run-adaptation').textContent = s.running || terminal
    ? `今回の予測誤差の学習補正：${s.peak_adaptation_enabled ? '使用する' : '使用しない'}`
    : '学習補正の設定は測定開始時に確定します。';
  $('run-percent').textContent = s.running || terminal
    ? `今回の入力：ZEROクロスで${s.input_current_mA} mA、入力時間は可変`
    : '通常入力はZEROクロスで行います。';
  $('run-target').textContent = s.running || terminal
    ? `今回の目標角度：${format(s.target_deg, 0)}°`
    : '目標角度は測定開始時に確定します。';
  $('run-brake').textContent = s.running || terminal
    ? s.energy_model === 'bridge' ? '今回の旧式換算：ブレーキなし' : `今回の負側ブレーキ上限：${s.brake_max_ms} ms`
    : '負側ブレーキ上限は測定開始時に確定します。';
  $('run-model').textContent = s.running || terminal
    ? s.energy_model === 'bridge' ? '今回の予測式：旧到達角予測をエネルギーに換算' : '今回の予測式：二次エネルギー式'
    : '予測式は測定開始時に確定します。';
  $('right').textContent = f.right_valid && (!stale || terminal) ? format(f.right_deg) + '°' : '—';
  $('left').textContent = f.left_valid && (!stale || terminal) ? format(f.left_deg) + '°' : '—';
  $('fps').textContent = terminal ? '停止中' : `${format(stale ? 0 : f.fps, 1)} / 15 fps`;
  $('foot-status').textContent = `足角度（任意） · ${f.zero_ready ? 'ゼロ点確定' : 'ゼロ点未確定（' + (f.zero_samples ?? 0) + '枚）'} · 記録${f.frames ?? 0}枚 · 画像取得失敗${f.failures ?? 0}回` +
    footIssues(f, stale, terminal);
  const reference = s.upright_reference;
  $('upright-reference-status').textContent = reference.capturing
    ? '基準を取得中です。約2秒間、今の姿勢で静止させてください。'
    : reference.valid
      ? `基準を保存済み（更新${reference.revision}回） · 基準からの左右角 ${format(reference.relative_pitch_deg)}° · 基準のMEKF角 ${format(reference.abs_pitch_deg)}°`
      : '基準は未保存です。機体を直立させ、今の姿勢を基準として保存してください。';
  if (s.running) $('guide').textContent = '測定中。画面は更新されます。足角度は制御入力に使用しません。';
  else if (reference.capturing) $('guide').textContent = '基準を取得しています。機体を動かさずお待ちください。';
  else if (!reference.valid) $('guide').textContent = '準備ができたら、直立姿勢を保って基準を保存してください。測定開始時には原点を取り直しません。';
  else if (s.downloadable) $('guide').textContent = 'ログを保存してください。次の測定には「ログを消去・次の測定へ」を使います。';
  else if (s.ready) $('guide').textContent = f.zero_ready && f.right_valid && f.left_valid && !stale
    ? '直立姿勢を保ち、測定を開始してください。'
    : '測定を開始できます。カメラ・足マーカーは任意です。足角度が取得できない区間は無効として記録します。';
  else $('guide').textContent = `IMUとモータの準備、直立姿勢を確認してください。足マーカーは開始条件ではありません。pitch基準差 ${format(s.upright?.pitch_error_deg, 1)}°（5°以内）/ 角速度 ${format(s.upright?.gyro_dps, 1)}°/s`;
  $('diagnostic-view').textContent = JSON.stringify(s, null, 2);
  if ($('usb-diag')) $('usb-diag').checked = s.usb_diagnostics === true;
  $('mekf-axes').textContent = s.mekf?.valid && s.mekf.fresh
    ? `前後 roll ${format(s.mekf.roll_deg)}° · 左右 pitch ${format(s.mekf.pitch_deg)}°`
    : 'MEKFの更新を待っています。';
  drawMarkers(f); controls();
}
async function refresh(force = false) {
  if (offlineMode && Date.now() < offlineUntil && !force) { renderOffline(); return; }
  if (refreshInFlight) return;
  refreshInFlight = true;
  const generation = offlineGeneration;
  let received = false;
  try {
    const s = await request('/status.json', {timeout: 2500}); received = true;
    // A pre-START poll can arrive after START entered quiet mode. Do not let
    // that old READY response restart automatic polling during the run.
    if (generation !== offlineGeneration) { if (offlineMode) renderOffline(); return; }
    if (s?.offline_run === true && Number.isFinite(s.wait_ms)) { setOffline(s.wait_ms); return; }
    const wasOffline = offlineMode;
    adoptStatus(s); clearOffline(); render(latest);
    if (wasOffline) $('message').textContent = s.network?.last_error || s.last_error ||
      (s.downloadable ? 'Web表示が復帰しました。ログを保存してください。' : s.command.result || 'Web表示が復帰しました。');
  } catch (error) {
    lastSeen = 0;
    if (offlineMode) { renderOffline(); return; }
    $('connection').textContent = received || error instanceof SyntaxError
      ? '状態データ・画面更新のエラー（自動再試行）'
      : '装置から応答がありません（自動再試行）';
    controls();
  } finally { refreshInFlight = false; }
}
function adoptStatus(s) {
  if (!s || typeof s.state !== 'string' || typeof s.export_phase !== 'string' ||
      !validInputPercent(s.input_peak_percent) || !validInputCurrent(s.input_current_mA) ||
      !brakeChoices.includes(s.brake_max_ms) ||
      typeof s.peak_adaptation_enabled !== 'boolean' ||
      !s.upright_reference || typeof s.upright_reference.valid !== 'boolean' ||
      typeof s.upright_reference.capturing !== 'boolean' ||
      (s.upright_reference.valid && (!Number.isFinite(s.upright_reference.abs_pitch_deg) ||
        !Number.isInteger(s.upright_reference.revision) || s.upright_reference.revision <= 0)) ||
      !s.foot || !s.upright || !s.command ||
      typeof s.command.pending !== 'boolean' ||
      !Number.isInteger(s.command.completed) || !Number.isInteger(s.command.submitted) ||
      ['running', 'ready', 'downloadable', 'controller_fresh'].some(key => typeof s[key] !== 'boolean'))
    throw new Error('装置の状態データが不完全です');
  latest = s; lastSeen = Date.now();
  if (poseSession && !poseSaved) {
    poseSession.trace.push(JSON.parse(JSON.stringify({client_time_ms:Date.now(), boot_id:s.boot_id,
      run_id:s.run_id, revision:s.revision, state:s.state, mekf:s.mekf, controller_fresh:s.controller_fresh,
      foot:{sequence:s.foot.sequence, age_ms:s.foot.age_ms, right_deg:s.foot.right_deg, left_deg:s.foot.left_deg,
        right_x:s.foot.right_x, left_x:s.foot.left_x, right_valid:s.foot.right_valid, left_valid:s.foot.left_valid}})));
    if (poseSession.trace.length > PoseComparison.limits.max_trace) { poseSession.trace.shift(); ++poseSession.trace_dropped; }
  }
}
async function poll() {
  try { await refresh(); }
  catch (error) { $('connection').textContent = '画面更新のエラー（自動再試行）'; }
  finally { setTimeout(poll, 800); }
}
async function startOfflineRun() {
  if (commandInFlight || offlineMode) return;
  const target = Number($('target').value);
  if (!targetChoices.includes(target)) {
    $('message').textContent = '目標角度を8°・10°・12°から選択してください。'; return;
  }
  const angleText = $('input-percent').value.trim();
  const inputPercent = Number(angleText);
  if (!angleText || !validInputPercent(inputPercent)) {
    $('message').textContent = 'ZEROクロス設定を確認してください。'; return;
  }
  const currentText = $('input-current').value.trim();
  const inputCurrentMa = Number(currentText);
  if (!currentText || !validInputCurrent(inputCurrentMa)) {
    $('message').textContent = '指令電流を100〜1200 mAの範囲で10 mA刻みで設定してください。'; return;
  }
  const energyModel = $('energy-model').value;
  if (!['bridge', 'quadratic'].includes(energyModel)) {
    $('message').textContent = '予測式を選択してください。'; return;
  }
  const brakeMaxMs = energyModel === 'bridge' ? 0 : Number($('brake-max').value);
  if (!brakeChoices.includes(brakeMaxMs)) {
    $('message').textContent = '負側ブレーキ上限を0・20・50 msから選択してください。'; return;
  }
  const adaptationValue = $('peak-adaptation').value;
  if (!['0', '1'].includes(adaptationValue)) {
    $('message').textContent = '予測誤差の学習補正の設定を確認してください。'; return;
  }
  requestedAdaptation = adaptationValue === '1';
  requestedTargetDeg = target;
  requestedInputPercent = inputPercent;
  requestedInputCurrentMa = inputCurrentMa;
  requestedBrakeMaxMs = brakeMaxMs;
  requestedEnergyModel = energyModel;
  commandInFlight = true; setOffline(30000); $('message').textContent = `左右揺動エネルギー制御（目標${target}°・${inputCurrentMa} mA）の開始要求を送信中…`;
  try {
    await request(`/start-energy-control-autonomous?target_deg=${target}&input_peak_percent=${inputPercent}&input_current_mA=${inputCurrentMa}&brake_max_ms=${brakeMaxMs}&peak_adaptation=${adaptationValue}&energy_model=${energyModel}`, {method:'POST', kind:'text'});
    $('message').textContent = `左右揺動エネルギー制御（目標${target}°・${inputCurrentMa} mA）の開始要求を受け付けました。Web休止後に本体が開始条件を確認します。Wi-Fi接続と画面をそのまま保ってお待ちください。`;
  } catch (error) {
    if (/^\d{3}:/.test(error.message)) { clearOffline(); await refresh(); }
    $('message').textContent = `開始結果の確認: ${error.message}。Web復帰後に本体の結果を確認します。`;
  } finally { commandInFlight = false; controls(); }
}
async function command(path) {
  const referenceCapture = path === '/upright-reference/capture';
  commandInFlight = true; controls(); $('message').textContent = referenceCapture
    ? '今の姿勢を約2秒間保ってください。基準を取得して保存します。' : '要求を送信中…';
  const before = latest?.command?.submitted ?? 0;
  try {
    await request(path, {method: 'POST', kind: 'text'});
    for (let i = 0; i < (referenceCapture ? 30 : 12); ++i) {
      const s = await request('/status.json'); adoptStatus(s); render(s);
      if (!s.command.pending && (s.command.completed > before || path === '/stop')) {
        $('message').textContent = referenceCapture ? referenceResultMessage(s.command.result) :
          s.command.result || '要求を処理しました'; return;
      }
      await nap(150);
    }
    $('message').textContent = '処理結果を確認中です。状態表示を確認してください。';
  } catch (error) {
    $('message').textContent = `通信結果が未確認です。状態を確認してください: ${error.message}`;
  } finally { commandInFlight = false; controls(); }
}
function referenceResultMessage(result) {
  const messages = {
    upright_reference_saved:'今の姿勢を直立基準として本体に保存しました。以後の測定でもこの基準を使います。',
    upright_reference_requires_stopped:'測定を停止してから基準を取得してください。',
    upright_reference_hold_still:'静止を確認できませんでした。元の基準を保っています。機体を静止させてやり直してください。',
    upright_reference_pose_moved:'取得中に角度が変わりました。元の基準を保っています。姿勢を保ってやり直してください。',
    upright_reference_imu_not_fresh:'姿勢の更新を確認できませんでした。元の基準を保っています。準備完了後にやり直してください。',
    upright_reference_save_failed:'本体への保存を確認できませんでした。基準を再取得してください。',
    upright_reference_timeout:'基準の取得が完了しませんでした。機体を静止させてやり直してください。',
    upright_reference_cancelled:'基準の取得を中止しました。元の基準を保っています。',
    stopped:'基準の取得を中止しました。元の基準を保っています。'
  };
  return messages[result] || `基準の取得結果：${result || '状態を確認してください'}`;
}
// A cache is optional for one-session downloads. IndexedDB additionally allows
// reload/reconnect resume; every cached chunk is revalidated before reuse.
let cachePromise;
const memoryCache = new Map();
function openCache() {
  if (!cachePromise) cachePromise = new Promise(resolve => {
    try {
      const r = indexedDB.open('freefoot-rwlog-v2', 1);
      r.onupgradeneeded = () => r.result.createObjectStore('chunks');
      r.onsuccess = () => resolve(r.result); r.onerror = () => resolve(null); r.onblocked = () => resolve(null);
    } catch { resolve(null); }
  });
  return cachePromise;
}
async function cacheGet(key) {
  const db = await openCache();
  if (!db) return memoryCache.get(key);
  return new Promise(resolve => {
    const tx = db.transaction('chunks', 'readonly'), r = tx.objectStore('chunks').get(key);
    r.onsuccess = () => resolve(r.result); r.onerror = () => resolve(undefined);
  });
}
async function cachePut(key, value) {
  memoryCache.set(key, value);
  const db = await openCache(); if (!db) return;
  await new Promise(resolve => {
    const tx = db.transaction('chunks', 'readwrite');
    tx.objectStore('chunks').put(value, key);
    tx.oncomplete = resolve; tx.onabort = resolve; tx.onerror = resolve;
  });
}
async function pruneCache(token) {
  const prefix = token + ':';
  for (const key of memoryCache.keys()) if (!key.startsWith(prefix)) memoryCache.delete(key);
  const db = await openCache(); if (!db) return;
  await new Promise(resolve => {
    try {
      const tx = db.transaction('chunks', 'readwrite'), r = tx.objectStore('chunks').openCursor();
      r.onsuccess = () => {
        const cursor = r.result;
        if (cursor) { if (!String(cursor.key).startsWith(prefix)) cursor.delete(); cursor.continue(); }
      };
      tx.oncomplete = resolve; tx.onabort = resolve; tx.onerror = resolve;
    } catch { resolve(); }
  });
}
function validateChunk(bytes, offset, length) {
  if (!(bytes instanceof Uint8Array) || bytes.byteLength !== length + 16) throw new Error('チャンク長の不一致');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint32(0, true) !== 0x31484346 || view.getUint32(4, true) !== offset || view.getUint32(8, true) !== length)
    throw new Error('チャンク識別情報の不一致');
  if (crc32(bytes.subarray(16)) !== view.getUint32(12, true)) throw new Error('チャンクCRC不一致');
  return bytes.subarray(16);
}
function saveBlob(blob, name) {
  const url = URL.createObjectURL(blob), a = document.createElement('a');
  a.href = url; a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(url), 60000);
}
function expandFootTable(value) {
  if (value == null || Array.isArray(value)) return value;
  const {fields, rows} = value;
  if (!Array.isArray(fields) || !fields.every(f => typeof f === 'string') ||
      new Set(fields).size !== fields.length || !Array.isArray(rows) ||
      rows.some(row => !Array.isArray(row) || row.length !== fields.length))
    throw new Error('足角度テーブルの形式が不正です');
  return rows.map(row => Object.fromEntries(fields.map((field,i) => [field,row[i]])));
}
async function download() {
  transferRunning = true; cancelTransfer = false; controls();
  try {
    $('transfer').textContent = 'ログを確定中…';
    await request('/export/prepare', {method: 'POST', kind: 'text'});
    let m;
    do {
      if (cancelTransfer) throw new Error('一時停止しました。取得・再開で続けられます。');
      m = await request('/export/manifest');
      if (m.phase === 'error') throw new Error(m.error);
      if (m.phase !== 'ready') { $('transfer').textContent = `ログ確定中 ${m.hashed_bytes} / ${m.bytes || '?'} bytes`; await nap(400); }
    } while (m.phase !== 'ready');
    if (!/^[a-f0-9]{16}$/.test(m.token) || !Number.isInteger(m.bytes) || m.bytes < 114 || m.bytes > 9 * 1024 * 1024 || m.chunk_bytes !== 4096)
      throw new Error('ログ仕様が一致しません。ページを更新してください。');
    await pruneCache(m.token);
    const file = new Uint8Array(m.bytes); $('progress').max = m.bytes; $('progress').value = 0;
    for (let offset = 0; offset < m.bytes;) {
      if (cancelTransfer) throw new Error('一時停止しました。取得・再開で続けられます。');
      const length = Math.min(m.chunk_bytes, m.bytes - offset), key = `${m.token}:${offset}`;
      let payload, packet = await cacheGet(key);
      if (packet) { try { payload = validateChunk(packet, offset, length); } catch { packet = null; } }
      for (let retry = 0; !payload && retry < 5; ++retry) {
        if (cancelTransfer) throw new Error('一時停止しました。');
        try {
          packet = await request(`/export/chunk?token=${m.token}&offset=${offset}&length=${length}`, {kind: 'bytes'});
          payload = validateChunk(packet, offset, length); await cachePut(key, packet);
        } catch (error) {
          if (retry === 4 || error.message.startsWith('409:')) throw error;
          $('transfer').textContent = `${offset} bytesまで取得済み · 再試行 ${retry + 1}/5`;
          await nap(250 * (retry + 1));
        }
      }
      file.set(payload, offset); offset += length; $('progress').value = offset;
      $('transfer').textContent = `${(100 * offset / m.bytes).toFixed(1)}% · ${offset} / ${m.bytes} bytes`;
    }
    const v = new DataView(file.buffer), crc = crc32(file.subarray(0, file.length - 4));
    if (crc !== m.crc32 || crc !== v.getUint32(file.length - 4, true)) throw new Error('ファイル全体のCRCが一致しません');
    const headerSize = v.getUint16(10, true), metadataLength = v.getUint32(24, true);
    if (headerSize !== 110 || metadataLength > file.length - headerSize - 4) throw new Error('RWLOGヘッダー不一致');
    const metadata = JSON.parse(new TextDecoder().decode(file.subarray(headerSize, headerSize + metadataLength)));
    completedFoot = expandFootTable(metadata.foot_frames ?? null); completedFile = file; completedName = m.filename;
    saveBlob(new Blob([file], {type: 'application/octet-stream'}), m.filename);
    $('transfer').textContent = `CRC検証完了 · ${m.bytes} bytes · 足角度 ${completedFoot?.length ?? 0}行`;
    calculateWheelWork();
  } catch (error) {
    $('transfer').textContent = `${error.message}（保存済みの部分は保持しています）`;
  } finally { transferRunning = false; controls(); }
}
function footCsv(rows) {
  if (!rows.length) return '';
  const keys = Object.keys(rows[0]);
  const esc = v => v == null ? '' : '"' + String(v).replaceAll('"', '""') + '"';
  return keys.map(esc).join(',') + '\r\n' + rows.map(row => keys.map(k => esc(row[k])).join(',')).join('\r\n') + '\r\n';
}
function validatePreviewManifest(m) {
  if (!m || m.format !== 'gray8' || m.width !== 160 || m.height !== 120 ||
      m.source_width !== 320 || m.source_height !== 240 || m.bytes !== 19200 ||
      !Number.isInteger(m.token) || m.token <= 0 || m.token > 0xffffffff ||
      !Number.isInteger(m.crc32) || m.crc32 < 0 || m.crc32 > 0xffffffff ||
      !Number.isInteger(m.sequence) || !m.right || !m.left)
    throw new Error('画像診断の形式が不正です');
}
function previewReason(reason) {
  return ({detected:'検出', no_frame:'画像なし', low_contrast:'明暗差不足', low_weight:'白領域不足',
    bad_shape:'幅が不適合', ambiguous:'候補が競合', track_jump:'位置が急変', reacquiring:'再捕捉中',
    waiting:'待機中', body_moving:'胴体の静止待ち', marker_invalid:'マーカー確認待ち',
    position_mismatch:'基準位置から外れています', marker_moving:'足の静止待ち',
    collecting:'取得中', ready:'確定'})[reason] ?? '未確認';
}
function drawPreview(m, pixels) {
  const small = document.createElement('canvas'); small.width = m.width; small.height = m.height;
  const smallCtx = small.getContext('2d'), bitmap = smallCtx.createImageData(m.width, m.height);
  for (let i = 0; i < pixels.length; ++i) {
    bitmap.data[i * 4] = bitmap.data[i * 4 + 1] = bitmap.data[i * 4 + 2] = pixels[i];
    bitmap.data[i * 4 + 3] = 255;
  }
  smallCtx.putImageData(bitmap, 0, 0);
  const canvas = $('preview-image'), ctx = canvas.getContext('2d');
  canvas.style.display = 'block'; ctx.imageSmoothingEnabled = false;
  ctx.drawImage(small, 0, 0, canvas.width, canvas.height);
  for (const [side, label] of [['right', 'A / 右足'], ['left', 'B / 左足']]) {
    const c = m[side];
    if (!c.candidates) continue;
    const x = c.x * 2, y = c.scan_y * 2;
    ctx.strokeStyle = ctx.fillStyle = c.valid ? '#43ff9c' : '#ffdc55'; ctx.lineWidth = 2;
    ctx.strokeRect(x - c.width, y - 16, c.width * 2, 32);
    ctx.beginPath(); ctx.arc(x, y, 6, 0, Math.PI * 2); ctx.stroke();
    ctx.font = 'bold 16px system-ui'; ctx.fillText(label, Math.max(4, Math.min(x + 10, 540)), Math.max(20, y - 22));
    if (c.alternate_x >= 0 && c.alternate_y >= 0) {
      ctx.strokeStyle = '#ffdc55'; ctx.beginPath(); ctx.arc(c.alternate_x * 2, c.alternate_y * 2, 9, 0, Math.PI * 2); ctx.stroke();
    }
  }
}
async function readPreviewPixels(m, cancelled = () => false) {
    const pixels = new Uint8Array(m.bytes);
    for (let offset = 0; offset < m.bytes; offset += 4096) {
      if (cancelled()) throw Error('取得を中止しました');
      const length = Math.min(4096, m.bytes - offset);
      const packet = await request(`/vision/chunk?token=${m.token}&offset=${offset}&length=${length}`, {kind:'bytes'});
      pixels.set(validateChunk(packet, offset, length), offset);
      await nap(20);
    }
    if (crc32(pixels) !== m.crc32) throw new Error('画像全体のCRCが不一致です');
    // Store exact gray pixels plus matching metadata in one downloadable JSON.
    // Never substitute the separately polled live status for this frame.
    let binary = ''; for (const value of pixels) binary += String.fromCharCode(value);
    return {...m, pixels_gray8_base64: btoa(binary)};
}
function showEvidence(evidence) {
    const {pixels_gray8_base64:binary,...m} = evidence;
    const pixels = Uint8Array.from(atob(binary), c => c.charCodeAt(0));
    drawPreview(m, pixels); previewEvidence = evidence;
    $('preview-status').textContent = `画像 #${m.sequence} · 取得要求の${format(m.age_ms, 0)} ms前のフレーム · 右 ${previewReason(m.right.reason)} / 左 ${previewReason(m.left.reason)} · ゼロ点 ${previewReason(m.zero_reason)}。姿勢を変えたら再取得してください。`;
}
async function capturePreview() {
  if (previewRunning || poseRunning || transferRunning || latest?.running) return;
  previewRunning = true; controls(); $('preview-status').textContent = '画像を取得中…';
  try {
    const m = await request('/vision/capture', {method:'POST'});
    validatePreviewManifest(m);
    showEvidence(await readPreviewPixels(m));
  } catch (error) {
    $('preview-status').textContent = `画像取得に失敗しました。${previewEvidence ? '表示と保存の対象は前回の画像です。' : ''}再取得してください: ${error.message}`;
  } finally { previewRunning = false; controls(); }
}
function renderPoses() {
  const tbody = $('pose-rows'); tbody.replaceChildren();
  if (!poseSession?.baseline) return;
  const rows = [{label:'直立基準',comparison:null},...poseSession.poses];
  for (const [i,row] of rows.entries()) {
    const c = row.comparison, tr = document.createElement('tr');
    const notes = {sideways_changed:'左右傾斜あり',outside_range:'設定範囲外',large_tilt:'傾斜大'};
    for (const value of [row.label || `姿勢 ${i}`,
      ...[c?.delta.roll_deg,c?.right_residual_deg,c?.left_residual_deg].map(v => c ? format(v) + '°' : '0.00°'),
      c ? (c.planar_check ? '比較用' : '参考：' + c.flags.map(f => notes[f]).join('・'))
        : '基準']) {
      const td = document.createElement('td'); td.textContent = value; tr.appendChild(td);
    }
    tbody.appendChild(tr);
  }
}
async function capturePose(baseline) {
  controls();
  if ($(baseline ? 'pose-base' : 'pose-add').disabled) return;
  if (baseline) poseSession = {schema:'freefoot-static-poses-v1', created_at:new Date().toISOString(),
    assumptions:['feet_fixed_to_ground','fore_aft_rotation_about_mekf_x'],
    residual_semantics:'delta_foot_body_relative_plus_delta_body_roll; planar approximation, not absolute foot attitude',
    timing_semantics:'camera exposure and delivery-time control snapshot differ; hold each pose stationary',
    trace_semantics:'existing status polls; not a high-rate IMU integration log',
    limits:PoseComparison.limits, baseline:null, poses:[], trace:[], trace_dropped:0};
  poseRunning = true; poseCancelled = false; poseSaved = false; controls();
  const frames = [];
  try {
    for (let i=0;i<PoseComparison.limits.samples;i++) {
      if (i) await nap(PoseComparison.limits.interval_ms);
      if (poseCancelled) throw Error('取得を中止しました');
      if (Date.now()-lastSeen >= 3500 || latest?.running || latest?.state !== 'READY_TO_MEASURE' ||
          !latest.controller_fresh || latest.command.pending) throw Error('接続と待機状態を確認してください');
      $('pose-status').textContent = `その姿勢を保ってください… ${i+1}/${PoseComparison.limits.samples}`;
      const m = await request('/vision/capture', {method:'POST'});
      validatePreviewManifest(m); PoseComparison.validateFrame(m); frames.push(m);
    }
    const summary = PoseComparison.summarize(frames, baseline);
    const comparison = baseline ? null : PoseComparison.compare(poseSession.baseline.summary, summary);
    const last = frames[frames.length-1];
    $('pose-status').textContent = '静止を確認しました。最後の画像を保存しています…';
    const image = await readPreviewPixels(last, () => poseCancelled);
    if (poseCancelled) throw Error('取得を中止しました');
    const record = {label:baseline ? '直立基準' : `姿勢 ${poseSession.poses.length+1}`, summary, comparison, frames, image};
    if (baseline) poseSession.baseline = record; else poseSession.poses.push(record);
    showEvidence(image); renderPoses();
    $('pose-status').textContent = baseline ? '基準を記録しました。足を固定したまま本体を傾け、「この姿勢を追加」を押してください。'
      : `姿勢 ${poseSession.poses.length} を記録しました。次の姿勢を追加するか、比較JSONを保存してください。`;
  } catch (error) {
    $('pose-status').textContent = error.message + ' 取得済みの姿勢は保持しています。';
    if (baseline && !poseSession.baseline) poseSession = null;
  } finally { poseRunning = false; controls(); }
}
$('start').onclick = startOfflineRun;
$('energy-model').onchange = controls;
$('upright-reference-capture').onclick = () => command('/upright-reference/capture');
if ($('stop')) $('stop').onclick = () => { poseCancelled = true; return command('/stop'); };
if ($('reconnect')) $('reconnect').onclick = () => refresh(true);
if ($('usb-diag')) $('usb-diag').onchange = async () => {
  const enabled = $('usb-diag').checked;
  commandInFlight = true; controls();
  try {
    await request(`/diagnostics/usb?enabled=${enabled ? 1 : 0}`, {method:'POST', kind:'text'});
    $('message').textContent = enabled ? 'USB診断を有効にしました。' : 'USB診断を停止しました。';
  } catch (error) { $('message').textContent = error.message; }
  finally { commandInFlight = false; await refresh(); }
};
$('clear').onclick = () => command('/clear');
$('download').onclick = download;
$('cancel').onclick = () => { cancelTransfer = true; };
$('work-calculate').onclick = calculateWheelWork;
$('work-pulses').onclick = () => saveWheelWorkCsv('pulses');
$('work-intervals').onclick = () => saveWheelWorkCsv('intervals');
$('work-kinematics').onclick = () => saveWheelWorkCsv('kinematics');
$('csv').onclick = () => saveBlob(new Blob(['\ufeff', footCsv(completedFoot)], {type: 'text/csv;charset=utf-8'}), completedName.replace(/\.rwlog$/, '_foot.csv'));
$('preview').onclick = capturePreview;
$('preview-save').onclick = () => saveBlob(new Blob([JSON.stringify(previewEvidence, null, 2)], {type:'application/json'}), `freefoot-vision-diagnostics-${previewEvidence.sequence}.json`);
$('diagnostics').onclick = () => saveBlob(new Blob([JSON.stringify(latest, null, 2)], {type: 'application/json'}), 'freefoot-diagnostics.json');
$('pose-base').onclick = () => capturePose(true);
$('pose-add').onclick = () => capturePose(false);
$('pose-cancel').onclick = () => { poseCancelled = true; };
$('pose-save').onclick = () => {
  saveBlob(new Blob([JSON.stringify(poseSession, null, 2)], {type:'application/json'}), 'freefoot-pose-comparison.json');
  poseSaved = true; controls();
};
$('pose-reset').onclick = () => {
  if ($('pose-reset').disabled) return;
  poseSession = null; poseSaved = false; renderPoses(); controls();
  $('pose-status').textContent = '胴体と両足を直立させ、基準を取得してください。';
};
try {
  const until = Number(sessionStorage.getItem(offlineKey));
  if (until > Date.now() && until <= Date.now() + 30000) setOffline(until - Date.now());
} catch (_) {}
poll();
