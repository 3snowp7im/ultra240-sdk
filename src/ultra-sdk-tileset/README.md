# Tileset binary file

* All values are little-endian and unsigned unless otherwise noted.
* All strings are NULL-terminated.
* All offsets are relative to the beginning of the file.

## Tileset format

* One short is the number of tiles.
* Two shorts are the width and height of each tile in pixels.
* One word is the offset of the image source string.
* One word is the offset of the library name string.
* One char is the number of tile data entries (_TDEn_) in this tileset.
* Next _TDEn_ words are the offsets of the tile data entries.

## Tile data entry format

* One short is the index of the tile associated with this data.
* One word is the name of this tile.
* One word is the offset of the library name string.
* One char is the number of collision box type headers (_CBTHn_) for this tile.
* Next _CBTHn_ words are the offsets of the collision box type headers.
* One char is the number of tiles (_ATn_) in the animation.
* Next 6 * _ATn_ bytes are the animation tiles.

## Collision box type header format

* One word is the collision box type.
* One short is the number of collision box lists (_CBLn_) for this type.
* Next _CBNHn_ words are the offsets of the collision box lists.

## Collision box list format

* One word is the collision box list name.
* One short is the number of collision boxes (_CBn_) for this list.
* Next 8 * _CBn_ bytes are the collision boxes for this list.

## Collision box format

* Two shorts are the x and y position of the top-left of the box in pixels.
* Two shorts are the width and height of the box in pixels.

## Animation tile format

* One short is the tile ID.
* One short is the duration in frames.
