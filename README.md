# fragments2mp4
fmp4 to mp4 converter

## Build
```make```

## Example
```./test.sh```

## Usage
First, extract metadata from the original mp4 file.
Then, feed this metadata to the convertor along with the fragments to get an mp4.

## Binaries
### extract_meta
Extract metadata from the original mp4 file

```Usage: ./extract_meta <mp4_filename> <metadata_filename>```
### fragments2mp4
Convert fragments back to mp4

```Usage: ./fragments2mp4 <mp4_filename> <meta_filename> <fmp4_1> [<fmp4_2>...]```
