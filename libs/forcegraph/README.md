# corbomite-forcegraph

A standalone Qt6/C++ force-directed graph visualization library.

CMake target `corbomite-forcegraph`, alias `Corbomite::ForceGraph`, public
headers under `corbomite/forcegraph/`, C++ namespace `Corbomite::ForceGraph`.

## Features

- Fruchterman-Reingold force-directed layout
- Barnes-Hut quadtree for O(n log n) repulsion
- QGraphicsView rendering with level-of-detail
- Interactive: pan, zoom, hover highlight, click, drag-to-pin
- Configurable physics parameters (center, repel, link forces)
- Targets 100-10,000 nodes

## License

GPL-3.0-or-later
