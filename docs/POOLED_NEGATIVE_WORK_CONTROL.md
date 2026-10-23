## 1.0.27: positive energy feedback and 30-second run

The work-control preset now records 30 seconds. Other normal and comparison presets retain their existing durations. Countdown, run termination and RWLOG metadata use the same duration.

For each positive prediction branch (historical angle-energy bridge or work model), store the last three completed matched residuals `r = U(actual positive peak) - E_raw_prediction` in mJ. After two observations use `b = clamp(mean(r), -0.12, +0.12) mJ`. Both histories reset each run. Failed writes and unmatched peaks are excluded.

For the work model minimize `abs(E_raw(input) + b - U(target))` over existing supported candidates. For the bridge fallback invert `E_raw(input) = U(target) - b`, retaining the current-response table and input limits. A negative residual requests more energy. The prior target-error integral is disabled in work-control mode to avoid adapting twice. Current integral is not treated as energy.

Store residuals against the raw prediction: subtract the applied bias when the work prediction already includes it. Bridge `predicted_energy_j` remains raw; `predicted_observed_energy_j` includes the correction. `prediction_bias_j` and `work_bias_samples` record both positive branches. Negative prediction and the 25/30/35/40 ms completed-peak gate are unchanged.

Causal replay of run 107786469 uses only earlier completed peaks. For six positive decisions after 10 seconds, energy prediction RMSE decreases from 0.05999 to 0.01668 mJ. See `positive_energy_feedback_replay.json` and the runtime math test. These are unchanged recorded inputs, not a closed-loop simulation or proof of reaching 8 degrees. Real response to changed inputs remains to be measured.

---

# 入力後の仕事を含む負側制御（1.0.26）

## 実機での手順

`1.0.26-pooled-negative-work`を書き込む。脚の前後移動を同じように固定し、保存した直立基準を確認して「仕事に基づく制御で15秒測定を開始」を押す。目標8°、負側300 mA。最初は1回測定してRWLOGを保存する。

## 今回のデータを反映した内容

77054774では負側25 msを9回実行し、後半5件の到達角は平均10.4646°だった。旧式の予測は平均10.1850°で、0.2796°小さく、RMSEは0.3187°だった。通電中の仕事は平均−0.3077 mJ、入力後からピークまでの仕事は+0.2195 mJ。正味の減速仕事を予測するには両方が必要である。

既存19回の0～25 ms測定412件、30～40 ms等を含む2群10回の248件、今回の25 ms後半5件、直前の無入力ログ42884626の後半5件を合わせ、31回670件で予測式を作った。今回の5件を丸ごと学習から外す検証のRMSEは0.1301°、31回のうち1回ずつを外す検証は全670件で0.3079°。最後に今回の5件も学習に含めた。どちらの検証も、入力後の実測仕事や次ピークを予測の入力に使わない。

## エネルギーと仕事の式

単位はmJ。`ω`はZEROクロス時の角速度[rad/s]、`r=(角速度[°/s]−70)/10`、`n=(ホイールrpm−200)/200`、`v=(電圧mV−7300)/500`、`p=直前の正側ピーク[°]−8`、`t=候補通電時間ms/25`。

```text
W_on  = −0.181207841t −0.110979567t² −0.013450859tr
        +0.029755850tn −0.002338393tv
W_off = 0.268238323 +0.029919620r +0.116990081n +0.003480068v
        −0.112391697t +0.043908940t² +0.007892802tr −0.063873554tn
ΔW    = W_on(t) + W_off(t) − W_off(0)
E_free = 0.290175920 +0.680478920ω² −0.014368250n
         +0.005002621p +0.012113272v
E_peak = E_free +0.441137878ΔW + b
```

`E_free`は無入力なら次の頂点に残るエネルギーの予測で、自然なホイールの減速の仕事も含む。`ΔW`は入力で変わる仕事で、無入力時の仕事を引いて二重計上を避ける。実測仕事と到達エネルギーの間の残差を`R=(0.441137878−1)ΔW+b`と置けば、`E_peak=E_free+ΔW+R`という収支になる。係数は暫定慣性、接触損失、計測誤差等を含む実験的な補正であり、独立に求めた機械効率ではない。

`b`は、完了した負側ピークの実測位置エネルギーと、補正前の予測エネルギーの差の直近3件平均。正側が7.5°以上保たれた有効な対応付きピークを使い、2件集まってから適用し、±0.12 mJに制限する。前回すでに加えた`b`を予測から除いて残差を計算するため、補正を二重に足さない。測定ごとに初期化する。

候補は0 ms、または10 msから現在の上限まで1 ms刻み。各候補で上記の式を計算し、`|U(8°)−E_peak|`が最も小さい入力を選ぶ。`U(8°)`は既存足裏円弧・重心モデルで約0.7334 mJ。入力後の仕事も含んで候補を比較するため、通電中だけで必要な仕事を満たしたとして計算を終えない。

## 強い入力への進め方

各測定の初期上限は25 ms。上限を使った入力の後でも、実測した負側のエネルギーが目標を0.03 mJ超えており、直前の正側ピークが7.5°以上だったことを2回続けて確認すると、候補の上限を25→30→35→40 msと1段階ずつ広げる。上限は選べる候補の範囲であり、上限の時間を必ず出力する指令ではない。

直前の正側が7.5°未満なら、その回の上限は25 ms。弱い正側に続く負側ピークが確認された場合も内部上限を25 msへ戻す。7°未満では従来どおり負側入力を見送る。実測で目標より0.03 mJ以上減らしすぎた場合は、上限を1段階下げる。40 msを超える指令は出さない。

## 適用範囲と未確認の部分

各変数の記録範囲は角速度57.2522～76.2778°/s、回転数4.28～575.79 rpm、電圧6.21～8.00 V、直前の正側ピーク5.0288～8.56178°。全組合せが測定済みという意味ではない。特に今回の低電圧で25 msを超える入力は、他条件の30～40 ms測定から推定する新しい組合せである。

25 msを超え、従来の30～40 ms測定の状態範囲に入らない候補は`work_prediction_cross_condition=true`とし、`energy_model_extrapolated`にも反映する。段階的な上限と正側の保護条件を使って、実機で確認する。オフライン検証は同じ条件の強い入力で8°を実現した証明ではない。今回の入力前状態を固定して40 msを予測しても、約9.6～9.9°である。閉ループでの変化は次の実機ログで確認する。

データの範囲外や古いホイール速度では負側の入力を見送る。正側は既存の制御を使う。計算はZEROクロス時の少数候補の評価と、ピーク確定時の3件平均だけで、マイコンで再学習しない。

## 記録と再現

`work_control_branch=4`が新しい負側の式。`prediction_bias_j`が適用した`b`、`work_bias_samples`が補正の観測数、`work_brake_cap_ms`がその回の上限。通電中・入力後・合計・無入力との差の予測仕事を別々に記録する。ピークの`model_residual_j`は適用後の予測との差で、補正用に内部で使う元の式との差とは区別する。

再現は`python tools/fit_pooled_negative_work.py`（numpy使用）。正規化した670行は`tools/fixtures/pooled_negative_31runs.csv`、検証と係数は`pooled_negative_31runs_validation.json`、元ログとSHA-256は`pooled_negative_31runs_sources.json`。元のRWLOGは変更しない。
