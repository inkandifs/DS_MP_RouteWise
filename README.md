# RouteWise

A graph-based route finder with a browser frontend and a C backend.

## Project parts

### Frontend
- `index.html`: page structure and forms
- `style.css`: visual design and responsive layout
- `script.js`: map drawing, button actions, and API calls

### Backend
- `server.c`: HTTP server, CSV loading/saving, graph, BFS, Dijkstra, queue, and min-heap

### Data storage
- `locations.csv`: location name and map coordinates
- `roads.csv`: connected locations, distance, and blocked status
- `history.csv`: previous route searches

### Server library
- `mongoose.c` and `mongoose.h`: Mongoose HTTP server library files

## One-time setup

1. Download `mongoose.c` and `mongoose.h` from the official Mongoose GitHub repository:
   - https://github.com/cesanta/mongoose
2. Put both files directly inside this `RouteWise` folder, beside `server.c`.
3. Install a C compiler that supports C11.
4. Open this folder in VS Code.

## Compile

Run this command in the VS Code terminal from inside the RouteWise folder:

```bash
gcc server.c mongoose.c -o routewise
```

On Windows with MinGW, the output executable may be named `routewise.exe`.

## Run

macOS / Linux:

```bash
./routewise
```

Windows:

```powershell
.\routewise.exe
```

Then open this address in your browser:

```text
http://localhost:8000
```

Keep the terminal running while using the website. Press Ctrl+C in the terminal to stop the server.

## How the algorithms work

- BFS uses a queue and finds a route with the fewest roads (edges). It does not optimize kilometres.
- Dijkstra uses a min-heap (priority queue) and finds the route with the lowest total distance.
- The graph is undirected: each road can be travelled in both directions.
- Blocked roads remain in `roads.csv`, but are ignored while searching for routes.

## Map coordinates

The map uses a simple SVG coordinate area:
- X: 20 to 580
- Y: 20 to 320

When adding a location, choose coordinates that do not overlap too much with existing locations.

## Important notes

- Location names should use letters, numbers, spaces, and underscores. Avoid commas.
- Distances must be positive whole numbers.
- The starter map is illustrative. Replace it with your own campus locations and road distances.
- CSV files are saved in the same folder as the running program. Run the server from this project folder.
- This is a local academic demonstration project, not a live navigation service.
