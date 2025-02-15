### **📌 ESP32 + FreeRTOS + PCA9685 を使用したロボットアーム制御**
ESP32 を使用し、**FreeRTOS で各軸を同時制御しながら Excel VBA で操作可能なロボットアーム制御システム**を構築しました。  
本リポジトリでは、**ESP32のプログラムとExcel VBAのスクリプト**を管理しています。  

現在、**非同期制御 (`main.ino`)** と **同期制御 (`main_sync.ino`)** の **2つのバージョン** を提供しています。

---

## 📘 詳しい情報はこちら
👉 [TANI-Lab公式ブログ](https://tani-lab.blogspot.com/)

🔗TANI-Labの商品購入先です。
👉 [TLP-A100 Amazon ](https://amzn.to/40O6x6r)  
👉 [TLP-A300 メルカリ ](https://jp.mercari.com/item/m35213370981)  
👉 [TLP-A300 Amazon ](https://amzn.to/3Cp6Jjg)  
👉 [TLP-A400 Amazon ](https://amzn.to/3Enzduf)  
👉 [TLP-P050 Amazon ](https://amzn.to/4hdklNI)  
👉 [TLP-P055 Amazon ](https://amzn.to/4gkuXcl)  
👉 [TLP-P120 Amazon ](https://amzn.to/3EhnI7D)  
👉 [TLP-P150 Amazon ](https://amzn.to/4jDk9sO)  

---

## **📌 環境情報**
| 項目 | 使用ハードウェア / ソフトウェア |
|------|--------------------------------|
| マイコン | ESP32 |
| 開発環境 | Arduino IDE / PlatformIO |
| ファームウェア | FreeRTOS |
| モーター制御 | PCA9685 (PWM制御) |
| サーボモーター | MG996R |
| 電源 | **Type-C一本**（TLP-A100でESP32に給電、TLP-P055でモーター用電力を供給） |
| 通信方式 | USB シリアル通信 |
| 制御方式 | Excel VBAマクロ（シリアル通信経由でコマンド送信） |
| プログラム公開 | **GitHubで管理** |

---

## **📌 プログラムについて**
- `main.ino` → **非同期制御バージョン**（各軸がバラバラに動作）  
- `main_sync.ino` → **同期制御バージョン**（全軸を同時に動作させることでスムーズな動きに）  

**`main_sync.ino` の同期制御のメリット**
✅ **すべてのサーボが同じタイミングで動作し、ロボットアーム全体の動きが滑らかに！**  
✅ **動作中の振動やブレを最小限に抑え、より精密な制御が可能！**  
✅ **電源負荷のバランスが最適化され、安定した動作を実現！**  

---

## **📌 機能**
✅ **各軸の同時制御（FreeRTOS によるマルチタスク処理）**  
✅ **移動完了の検知（シリアル通信で `DONE` メッセージ送信）**  
✅ **Excel VBAで直感的に操作可能**  
✅ **Type-C一本で駆動（TLP-A100 + TLP-P055使用）**  
✅ **ESP32のプログラムはGitHubで管理**  

---

## **📌 プログラムのセットアップ**
### **1. ESP32にプログラムを書き込む**
#### **📌 必要なライブラリ**
Arduino IDE で以下のライブラリをインストールしてください。
- `Adafruit_PWMServoDriver` （PCA9685用）
- `FreeRTOS` （ESP32には標準で搭載）

#### **📌 書き込み手順**
1. ESP32をPCに接続  
2. `main.ino`（非同期制御） または `main_sync.ino`（同期制御） を Arduino IDE または PlatformIO で開く  
3. **ボード設定:**  
   - ボード: `ESP32 Dev Module`  
   - フラッシュサイズ: `4MB`  
   - シリアルポートを選択  
4. コンパイル & 書き込み  

---

### **2. Excel VBAで制御**
#### **📌 事前準備**
- Excelの `開発タブ` を有効にする  
- VBAエディタ (`Alt + F11`) を開き、`シリアル通信モジュール` を追加  
- `COMポート` を適切に設定  

#### **📌 Excel VBAのサンプルコード**
```vba
Sub MoveServo()
    Dim serialPort As Object
    Set serialPort = CreateObject("MSCommLib.MSComm")
    
    ' シリアルポート設定 (適宜変更)
    serialPort.CommPort = 3
    serialPort.Settings = "115200,N,8,1"
    serialPort.PortOpen = True
    
    ' コマンド送信
    serialPort.Output = "MOVE 0 45" & vbCrLf
    serialPort.Output = "MOVE 1 90" & vbCrLf
    serialPort.Output = "MOVE 2 120" & vbCrLf
    
    serialPort.PortOpen = False
    Set serialPort = Nothing
End Sub
```

#### **📌 コマンド一覧**
| コマンド | 説明 |
|----------|------|
| `MOVE <ch> <angle>` | `<ch>` チャンネルのサーボを `<angle>` 度に移動 |
| `GETPOS` | 現在の角度を取得（`POS 90,90,90,90,90,0` の形式で返す） |
| `SYNC MOVE <ch> <angle> ...` | **複数軸を同時に移動（`main_sync.ino` のみ）** |
| `DONE SYNC` | **`SYNC MOVE` の完了通知（`main_sync.ino` のみ）** |

---

## **📌 使用例**
1. **ESP32に `main_sync.ino` を書き込む**
2. **Excel VBA で `MoveServo()` を実行**
3. **ロボットアームが滑らかに動く！**
4. **完了後に `DONE SYNC` メッセージが表示される**
5. **`GETPOS` で現在の角度を確認**

---

## **📌 今後のアップデート予定**
- ✅ **GUIを作成し、Excelから直感的に操作可能にする**  
- ✅ **複数のモーションを事前に登録し、自動実行できる機能の追加**  
- ✅ **GitHubのリードミーを更新し、詳細な解説を追加**  

---

## **📌 関連リンク**
📢 **GitHubリポジトリ:** [ESP32 + FreeRTOS + PCA9685でロボットアーム制御](https://github.com/TANI-Lab/ROBARM)  
📢 **TANI-LabのYouTube:** [TANI-Labチャンネル](https://www.youtube.com/@TANI-Lab/featured)  

---

## **📌 ライセンス**
本プロジェクトは **MITライセンス** のもと公開されています。自由に使用・改変できますが、再配布の際はライセンス表記をお願いします。


