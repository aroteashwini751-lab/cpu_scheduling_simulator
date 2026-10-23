
const $ = (id) => document.getElementById(id);

let nextPid = 1;

function addProcess(pid = `P${nextPid++}`, arrival = 0, burst = 5, priority = 1) {
  const tr = document.createElement("tr");

  const values = [pid, arrival, burst, priority];

  values.forEach((value, index) => {
    const td = document.createElement("td");
    const input = document.createElement("input");

    input.value = value;
    input.type = index === 0 ? "text" : "number";
    input.setAttribute("aria-label",
      ["Process ID", "Arrival time", "Burst time", "Priority"][index]);

    if (index === 0) {
      input.maxLength = 12;
      input.pattern = "[A-Za-z0-9_-]+";
      input.required = true;
    } else {
      input.min = index === 2 ? "1" : "0";
      input.step = "1";
      input.required = true;
    }

    td.appendChild(input);
    tr.appendChild(td);
  });

  const action = document.createElement("td");
  const remove = document.createElement("button");
  remove.textContent = "Remove";
  remove.type = "button";
  remove.className = "delete";
  remove.addEventListener("click", () => tr.remove());
  action.appendChild(remove);
  tr.appendChild(action);

  $("processRows").appendChild(tr);
}

function loadSample() {
  $("processRows").replaceChildren();
  nextPid = 1;

  [
    ["P1", 0, 7, 2],
    ["P2", 2, 4, 1],
    ["P3", 4, 1, 3],
    ["P4", 5, 4, 2]
  ].forEach(p => addProcess(...p));

  $("algorithm").value = "AUTO";
  $("quantum").value = 2;
  $("status").textContent = "";
}

function readProcesses() {
  const rows = [...$("processRows").querySelectorAll("tr")];

  if (!rows.length) {
    throw new Error("Add at least one process.");
  }

  if (rows.length > 100) {
    throw new Error("Please use 100 or fewer processes.");
  }

  const seen = new Set();

  return rows.map((row) => {
    const inputs = [...row.querySelectorAll("input")];
    const [pidInput, arrivalInput, burstInput, priorityInput] = inputs;

    const pid = pidInput.value.trim();
    const arrival = Number(arrivalInput.value);
    const burst = Number(burstInput.value);
    const priority = Number(priorityInput.value);

    if (!/^[A-Za-z0-9_-]{1,12}$/.test(pid)) {
      throw new Error("Process IDs must contain only letters, numbers, _ or -.");
    }

    if (seen.has(pid)) {
      throw new Error(`Duplicate process ID: ${pid}`);
    }
    seen.add(pid);

    if (
      !Number.isSafeInteger(arrival) || arrival < 0 ||
      !Number.isSafeInteger(burst) || burst < 1 ||
      !Number.isSafeInteger(priority) || priority < 0 ||
      arrival > 1000000 || burst > 1000000 || priority > 1000000
    ) {
      throw new Error(
        "Use integer arrival times and priorities from 0 to 1000000, " +
        "and burst times from 1 to 1000000."
      );
    }

    return { pid, arrival, burst, priority };
  });
}

function renderResults(data) {
  $("recommendation").replaceChildren();

  const heading = document.createElement("strong");
  heading.textContent = data.recommendation;
  $("recommendation").appendChild(heading);

  const reason = document.createElement("p");
  reason.textContent = data.reason;
  $("recommendation").appendChild(reason);

  $("algorithmUsed").textContent =
    `Algorithm used: ${data.algorithm}`;

  $("avgWaiting").textContent = data.avgWaiting.toFixed(2);
  $("avgTurnaround").textContent = data.avgTurnaround.toFixed(2);
  $("utilization").textContent = `${data.utilization.toFixed(2)}%`;

  const gantt = $("gantt");
  gantt.replaceChildren();

  data.gantt.forEach(segment => {
    const block = document.createElement("div");
    block.className = "gantt-block" +
      (segment.pid === "IDLE" ? " idle" : "");

    const label = document.createElement("b");
    label.textContent = segment.pid;

    const times = document.createElement("small");
    times.textContent = `${segment.start} – ${segment.end}`;

    block.append(label, times);
    block.style.width =
      `${Math.max(70, (segment.end - segment.start) * 35)}px`;

    gantt.appendChild(block);
  });

  const results = $("results");
  results.replaceChildren();

  data.processes.forEach(p => {
    const tr = document.createElement("tr");

    [
      p.pid, p.arrival, p.burst, p.priority,
      p.completion, p.turnaround, p.waiting, p.response
    ].forEach(value => {
      const td = document.createElement("td");
      td.textContent = value;
      tr.appendChild(td);
    });

    results.appendChild(tr);
  });
}

$("addProcess").addEventListener("click", () => addProcess());

$("sample").addEventListener("click", loadSample);

$("run").addEventListener("click", async () => {
  const button = $("run");
  $("status").textContent = "";

  try {
    const processes = readProcesses();
    const quantum = Number($("quantum").value);

    if (!Number.isSafeInteger(quantum) || quantum < 1 || quantum > 1000000) {
      throw new Error("Time quantum must be an integer from 1 to 1000000.");
    }

    button.disabled = true;
    button.textContent = "Running...";

    const response = await fetch("/simulate", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        algorithm: $("algorithm").value,
        quantum,
        processes
      })
    });

    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.error || "Simulation failed.");
    }

    renderResults(data);
  } catch (error) {
    $("status").textContent = error.message;
  } finally {
    button.disabled = false;
    button.textContent = "Run Simulation";
  }
});

addProcess("P1", 0, 5, 2);
addProcess("P2", 1, 3, 1);
addProcess("P3", 2, 6, 3);
