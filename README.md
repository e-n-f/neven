neven
=====

The Neven Vision face detector from Android

And some very messy code from about 10 years ago to call it and
output the bounding boxes of the faces

## Usage

```
./neven-face-detect file.jpg file2.jpg ...
```

Writes lines like

```
0 482x482+1004+400 1358,520 1358,520 1132,520 1132,520 1245,761 -- 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 file.jpg
```

with the bounding box and landmarks of the detected faces.
