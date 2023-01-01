# Aseprite Player for Playdate

[Japanese](README_ja.md)

This library allows data created with Aseprite to be played back with Playdate.

## samples

### setup

Be sure to do the following first.

```
cd sample
rake generate:resource
```

### execute each sample

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

Open and run the generated .xcodeproject.

