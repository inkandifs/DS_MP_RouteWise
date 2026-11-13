// RouteWise frontend logic.
// The browser talks to the C backend through simple HTTP API requests.

let mapData = { locations: [], roads: [] };
let selectedPath = [];

const byId = (id) => document.getElementById(id);

// Build a URL with query parameters safely encoded.
function apiUrl(endpoint, values = {}) {
  const query = new URLSearchParams(values);
  return `/api/${endpoint}?${query.toString()}`;
}

async function request(endpoint, values = {}) {
  const response = await fetch(apiUrl(endpoint, values));
  const data = await response.json();
  if (!response.ok || data.ok === false) {
    throw new Error(data.error || "The request could not be completed.");
  }
  return data;
}

function fillSelect(select, locations) {
  const oldValue = select.value;
  select.innerHTML = "";
  locations.forEach((location) => {
    const option = document.createElement("option");
    option.value = location.name;
    option.textContent = location.name;
    select.appendChild(option);
  });
  if (locations.some((location) => location.name === oldValue)) {
    select.value = oldValue;
  }
}

async function loadMapData() {
  mapData = await request("data");
  fillSelect(byId("source"), mapData.locations);
  fillSelect(byId("destination"), mapData.locations);
  fillSelect(byId("roadFrom"), mapData.locations);
  fillSelect(byId("roadTo"), mapData.locations);
  drawMap();
  renderRoads();
}

function drawMap() {
  const svg = byId("map");
  svg.innerHTML = "";

  // Draw roads first so location circles appear above the lines.
  mapData.roads.forEach((road) => {
    const a = mapData.locations.find((item) => item.name === road.from);
    const b = mapData.locations.find((item) => item.name === road.to);
    if (!a || !b) return;

    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", a.x);
    line.setAttribute("y1", a.y);
    line.setAttribute("x2", b.x);
    line.setAttribute("y2", b.y);

    const isSelected = selectedPath.length > 1 &&
      selectedPath.some((name, index) =>
        index < selectedPath.length - 1 &&
        ((name === road.from && selectedPath[index + 1] === road.to) ||
         (name === road.to && selectedPath[index + 1] === road.from))
      );

    line.setAttribute("class", `road${road.blocked ? " blocked" : ""}${isSelected ? " selected" : ""}`);
    svg.appendChild(line);

    const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
    label.setAttribute("x", (a.x + b.x) / 2);
    label.setAttribute("y", (a.y + b.y) / 2 - 7);
    label.setAttribute("class", "road-label");
    label.textContent = `${road.distance} km`;
    svg.appendChild(label);
  });

  // Draw each location as a circle and label.
  mapData.locations.forEach((location) => {
    const group = document.createElementNS("http://www.w3.org/2000/svg", "g");
    const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
    circle.setAttribute("cx", location.x);
    circle.setAttribute("cy", location.y);
    circle.setAttribute("r", 8);
    circle.setAttribute("class", `node${selectedPath.includes(location.name) ? " selected" : ""}`);

    const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
    label.setAttribute("x", location.x);
    label.setAttribute("y", location.y - 15);
    label.setAttribute("class", "node-label");
    label.textContent = location.name;

    group.appendChild(circle);
    group.appendChild(label);
    svg.appendChild(group);
  });
}

function renderRoads() {
  const list = byId("roadList");
  list.innerHTML = "";

  if (mapData.roads.length === 0) {
    list.textContent = "No roads have been added yet.";
    return;
  }

  mapData.roads.forEach((road) => {
    const row = document.createElement("div");
    row.className = "road-row";

    const details = document.createElement("div");
    const title = document.createElement("strong");
    title.textContent = `${road.from} ↔ ${road.to}`;
    const subtitle = document.createElement("small");
    subtitle.textContent = `${road.distance} km · ${road.blocked ? "Blocked" : "Open"}`;
    details.append(title, subtitle);

    const button = document.createElement("button");
    button.textContent = road.blocked ? "Restore" : "Block";
    if (road.blocked) button.classList.add("unblock");
    button.addEventListener("click", async () => {
      try {
        await request("toggle-road", {
          from: road.from,
          to: road.to,
          blocked: road.blocked ? "0" : "1"
        });
        selectedPath = [];
        await loadMapData();
      } catch (error) {
        alert(error.message);
      }
    });

    row.append(details, button);
    list.appendChild(row);
  });
}

function showResult(message, type = "empty") {
  const box = byId("routeResult");
  box.className = `result ${type}`;
  box.textContent = message;
}

async function findRoute() {
  const source = byId("source").value;
  const destination = byId("destination").value;
  const algorithm = byId("algorithm").value;

  if (!source || !destination) {
    showResult("Add at least two locations before searching.", "error");
    return;
  }

  try {
    const result = await request("route", { source, destination, algorithm });
    if (!result.found) {
      selectedPath = [];
      drawMap();
      showResult(result.message, "error");
      return;
    }

    selectedPath = result.path;
    drawMap();

    const unit = algorithm === "bfs" ? "roads (hops)" : "km";
    const label = algorithm === "bfs" ? "BFS route" : "Shortest-distance route";
    showResult(`${label}\n${result.path.join(" → ")}\nTotal: ${result.distance} ${unit}`, "success");
    await loadHistory();
  } catch (error) {
    showResult(error.message, "error");
  }
}

async function loadHistory() {
  try {
    const history = await request("history");
    const list = byId("historyList");
    list.innerHTML = "";

    if (!history.length) {
      list.textContent = "No searches yet.";
      return;
    }

    history.slice().reverse().forEach((item) => {
      const row = document.createElement("div");
      row.className = "history-row";
      const title = document.createElement("strong");
      title.textContent = `${item.source} → ${item.destination}`;
      const detail = document.createElement("span");
      detail.textContent = `${item.algorithm.toUpperCase()} · ${item.distance} ${item.algorithm === "bfs" ? "roads" : "km"} · ${item.path}`;
      row.append(title, detail);
      list.appendChild(row);
    });
  } catch (error) {
    byId("historyList").textContent = error.message;
  }
}

byId("findRoute").addEventListener("click", findRoute);
byId("refreshMap").addEventListener("click", async () => {
  selectedPath = [];
  await loadMapData();
});

byId("locationForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    await request("add-location", {
      name: byId("locationName").value.trim(),
      x: byId("locationX").value,
      y: byId("locationY").value
    });
    event.target.reset();
    await loadMapData();
    showResult("Location added to the map.", "success");
  } catch (error) {
    alert(error.message);
  }
});

byId("roadForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    await request("add-road", {
      from: byId("roadFrom").value,
      to: byId("roadTo").value,
      distance: byId("roadDistance").value
    });
    event.target.reset();
    await loadMapData();
    showResult("Road added to the graph.", "success");
  } catch (error) {
    alert(error.message);
  }
});

byId("clearHistory").addEventListener("click", async () => {
  if (!confirm("Clear all saved route history?")) return;
  try {
    await request("clear-history");
    await loadHistory();
  } catch (error) {
    alert(error.message);
  }
});

// Load saved CSV data when the page first opens.
async function startApp() {
  try {
    await loadMapData();
    await loadHistory();
  } catch (error) {
    showResult(`Could not connect to the C server: ${error.message}`, "error");
  }
}
startApp();
