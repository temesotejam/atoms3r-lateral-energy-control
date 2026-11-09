#!/usr/bin/env python3
"""Create an identified offline-localhost flasher around CI-built binaries."""
import argparse
import hashlib
import html
import json
from pathlib import Path

MODE_NAMES = {
    0: 'Mode 0: 従来の撮影待機',
    1: 'Mode 1: 撮影待機を削除',
    2: 'Mode 2: 撮影待機を削除 + フレームをコピーして早期返却',
}
MODE_NOTES = {
    0: 'Mode 0 は静置診断専用です。非ゼロ電流指令を禁止し、受動観測します。実機未検証です。',
    1: 'Mode 1 は静置診断専用です。非ゼロ電流指令を禁止し、受動観測します。実機未検証です。',
    2: 'Mode 2 は静置診断専用です。非ゼロ電流指令を禁止し、受動観測します。実機未検証です。',
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=int, choices=MODE_NAMES, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--checkout-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    for sha in (args.commit, args.checkout_commit):
        if len(sha) != 40 or any(c not in '0123456789abcdef' for c in sha):
            parser.error('commits must be 40 lowercase hexadecimal characters')
    root = args.output
    files = [('firmware/bootloader.bin', 0), ('firmware/partitions.bin', 32768),
             ('firmware/boot_app0.bin', 57344), ('firmware/firmware.bin', 65536)]
    sums = []
    for name, _ in files:
        data = (root / name).read_bytes()
        if not data:
            raise ValueError('Empty flash image: ' + name)
        sums.append(hashlib.sha256(data).hexdigest() + '  ' + name)
    manifest = {'name': 'AtomS3R カメラ比較用 ' + MODE_NAMES[args.mode],
                'version': f'camera-compare-mode-{args.mode}-{args.commit[:12]}',
                'new_install_improv_wait_time': 0,
                'builds': [{'chipFamily': 'ESP32-S3', 'parts': [
                    {'path': name, 'offset': offset} for name, offset in files]}]}
    (root / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (root / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='utf-8')
    (root / 'BUILD.txt').write_text(f'mode={args.mode}\npr_head={args.commit}\ncheckout={args.checkout_commit}\n', encoding='utf-8')
    title = html.escape(MODE_NAMES[args.mode])
    note = html.escape(MODE_NOTES[args.mode])
    sha = html.escape(args.commit)
    page = f'''<!doctype html><html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>カメラ比較用 {title}</title>
<script type="module" src="https://unpkg.com/esp-web-tools@10.1.1/dist/web/install-button.js?module"></script>
<style>body{{font:16px/1.7 system-ui,sans-serif;max-width:760px;margin:3rem auto;padding:0 1rem}} button{{font-size:1.1rem;padding:.8rem 1rem}}code{{word-break:break-all}}</style></head>
<body><h1>カメラ比較用ファームウェア</h1><h2>{title}</h2>
<p>研究用の比較版です。通常版の公開ページは変更されません。実機での速度や処理負荷は未検証です。</p><p><strong>{note}</strong></p>
<p>静置したまま60秒間データを取得します。これに加えて同期とログ転送の時間が必要です。画面に従来の30秒表示が出ても途中で終了せず、取得とログ転送の完了まで待ってください。</p>
<p>PRのコミット: <code>{sha}</code></p>
<p>PCのChromeまたはEdgeで、機体をUSB接続し、下のボタンからポートを選択してください。ボタンの読み込みにはインターネット接続が必要です。</p>
<esp-web-install-button manifest="manifest.json"><button slot="activate">このモードを書き込む</button><span slot="unsupported">ChromeまたはEdgeを利用してください。</span></esp-web-install-button>
<p>ログを保存する際は、このモード番号とコミットを一緒に記録してください。</p></body></html>'''
    (root / 'index.html').write_text(page, encoding='utf-8')
    (root / 'README.txt').write_text(f'''カメラ比較ファームウェア: {MODE_NAMES[args.mode]}
{MODE_NOTES[args.mode]}
PR head: {args.commit}
CIで実際にチェックアウトしたコミット: {args.checkout_commit}

このZIP全体を一つのフォルダーに展開してください。
静置したまま60秒間データを取得し、さらに同期とログ転送の時間がかかります。
従来の30秒表示が出ても途中で終了せず、取得とログ転送が完了するまで待ってください。
WindowsのPowerShellでそのフォルダーに移動し、次を実行します。
  py -m http.server 8766 --bind 127.0.0.1
PCのChrome/Edgeで http://127.0.0.1:8766/ を開いて書き込みます。
書込み用のWeb部品を読み込むため、インターネット接続も必要です。
py が使えない場合は python -m http.server 8766 --bind 127.0.0.1 を試してください。
終わったらPowerShellで Ctrl+C を押してサーバーを停止します。
別モードは別のZIPから起動してください。通常版のWeb書込み画面は別に残ります。
フラッシュ部品のハッシュは SHA256SUMS.txt を参照してください。
''', encoding='utf-8')


if __name__ == '__main__':
    main()
