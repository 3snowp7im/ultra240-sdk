# World binary file

* All values are little-endian and unsigned unless otherwise noted.
* All positions and dimensions are expressed as multiples of the map tile size
  (16x16 pixels) unless otherwise noted.
* All offsets are relative to the beginning of the file.

## World file header format

* One short is the number of maps in the world (_Mn_).
* Next _Mn_ words are the offsets for each map header.
* One short is the number of boundaries (_Bn_).
* Next _Bn_ words are the offsets for each boundary.

## Map header format

* Two signed shorts are the x and y position of the map.
* Two shorts are the width (_Mw_) and height (_Mh_) of the map.
* One char is the number of map tilesets (_MTSn_) in the map.
* Next _MTSn_ words are the offsets of the map tilesets.
* One char is the number of entity tilesets (_ETSn_) in the map.
* Next _ETSn_ words are the offsets of the entity tilesets.
* One char is the number of map tile layers (_Ln_) in the map.
* Next _Ln_ words are the offsets of the map tile layers.
* One char is the layer index where entities should be rendered.
* One short is the number of entities in the map (_En_).
* Next 10 * _En_ chars are the entities in the map.
* Next _En_ shorts are the indices of each entity in the map in ascending order
  of their position's x component.
* Next _En_ shorts are the indices of each entity in the map in ascending order
  of the sum of their position's x component and their tile width.
* Next _En_ shorts are the indices of each entity in the map in ascending order
  of their position's y component.
* Next _En_ shorts are the indices of each entity in the map in ascending order
  of the sum of their position's y component and their tile height.

## Tileset format

Documented in the [ultra-sdk-tileset](../ultra-sdk-tileset/README.md) tool
source directory. Most importanly, it contains:

* The width (_Tw_) and height (_Th_) of each tile in pixels.
* The number of columns (_TScn_) of the tileset. This must be inferred by
  dividing the pixel width of the tileset's source image by _Tw_.

## Layer format

* Two chars are the numerator and denominator of the layer parallax in the x
  dimension.
* Two chars are the numerator and denominator of the layer parallax in the y
  dimension.
* Next _Mw_ * _Mh_ shorts are the layer tiles listed from top-left to bottom-
  right.
  
## Entity format

* Two shorts are the x and y position of the entity in pixels.
* One short is the entity tile.
* One word is the entity state.

## Tiles

* The first 4 bits of the tile are the tileset index. This limits the number of
  tilesets attached to a map to 32, that being 16 for map tile tilesets and 16
  for entity tilesets.
* For map tiles, the remaining 12 bits are the tile ID (_Tid_). This limits the
  number of usable map tiles in a tileset to 4096.
* For entity tiles, the next 2 bits are the flip x and flip y attributes
  respectively. The remaining 10 bits are the tile ID (_Tid_). This limits the
  number of usable entity tiles in a tileset to 1024.

### Additional nodes on tiles

* __Zero values are always empty and unrendered.__
* The image that should be rendered for that tile will be a _Tw_ by _Th_ pixel
  rectangle at the (x,y) coordinates of the image referenced by the tileset
  where:
  > x = _Tw_ * (_Tid_ % _TScn_)  
  > y = _Th_ * floor(_Tid_ / _TScn_)
* All map tileset tiles must be 16 x 16 pixels.
* Entity tileset tiles may be any size.
* Map and entity layers may share tilesets. Consequently, the offset to a map
  tileset may appear in the list of entity tileset offsets, however, the tiles'
  tileset indices may differ between map and entity layers.

## Boundary format

* One char are the flags for this boundary.
* One short is the number of lines comprising this boundary (_BLn_).
* Next _BLn_ * 8 bytes are the points comprising the boundary lines.

## Boundary flags

| Value | Name   | Description                                                 |
| ----- | ------ | ----------------------------------------------------------- |
| 0x40  | OneWay | Collision is checked when the dot product of the entity's   |
|       |        | direction vector and the boundary's normal is positive.     |
|       |        | This results in entities passing through the boundary in    |
|       |        | one direction, but colliding with the other.                |

## Boundary point format

* Two signed words represent the x and y values of the point in pixels.

### Additional notes on boundaries

* Each point in the list of points comprising the boundary lines is connected
  to the next point in the list.
