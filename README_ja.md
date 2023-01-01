# Aseprite Player for Playdate

Asepriteで作成したデータをPlaydateで再生できるライブラリです。

## サンプル

### setup

最初に必ず以下を実行してください。

```
cd sample
rake setup
```

### 各サンプルを実行してみる

#### コマンドラインで実行

```
cd sample/simple_file
rake generate:simulator:debug
rake build:simulator:debug
rake run:debug
```

#### Xcodeで実行

```
cd sample/simple_file
rake generate:xcode
```

生成される.xcodeprojectを開いて実行します。

