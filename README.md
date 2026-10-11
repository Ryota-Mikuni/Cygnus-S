# Cygnus-S — 無線キーボードのファームウェア

**分類: キーボード / ZMKファームウェア**

無線キーボードCygnusのファームウェアです。使い方などは別途添付のマニュアルをご参照ください。
このリポジトリは [Dist16384/Cygnus-S](https://github.com/Dist16384/Cygnus-S) のフォークです。

## 目的別の入口

| 確認・変更したいこと | 場所 |
|---|---|
| キー配列・レイヤー | [config/Cygnus.keymap](config/Cygnus.keymap) / [関連JSON](config/Cygnus.json) |
| 左右のハードウェア定義 | [config/boards/shields/Test/](config/boards/shields/Test/) |
| ジェスチャー処理 | [src/](src/) / [config/src/](config/src/) |
| 依存するZMK・モジュール | [config/west.yml](config/west.yml) |
| ビルド対象 | [build.yaml](build.yaml) / [GitHub Actionsの定義](.github/workflows/build.yml) |
| 設計・引き継ぎ・表示資料 | [docsの一覧](docs/README.md) |
| 作業ブランチと未統合PR | [ブランチ台帳](docs/BRANCHES.md) |

## ビルド対象と確認の順序

`build.yaml` は `seeeduino_xiao_ble` 向けの `Cygnus_R rgbled_adapter`、`Cygnus_L rgbled_adapter`、`settings_reset` を定義しています。
キー配列と対象ブランチを確認し、GitHub Actionsの対応する実行結果と生成物を確認してから機種に合うファームウェアを選びます。
左右の設定や設定リセット用の対象は、別々に扱います。

## 現在の作業状態

2026-10-11の確認では [操作拡張のPR #1](https://github.com/Ryota-Mikuni/Cygnus-S/pull/1) が未統合です。mainと実験ブランチの対応はブランチ台帳を参照してください。
[ジェスチャー引き継ぎ資料](docs/CODEX_HANDOFF_CYGNUS_GESTURE.md) の最終更新は2026-06-01です。そこに記載されたビルド結果を、今回の新しいビルドや現在の実機検証結果として扱いません。
