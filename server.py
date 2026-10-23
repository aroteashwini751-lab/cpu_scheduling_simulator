
import json
import os
import re
import subprocess
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
EXE = BASE_DIR / ("scheduler.exe" if os.name == "nt" else "scheduler")
HOST = "127.0.0.1"
PORT = 8000


class SimulatorHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BASE_DIR), **kwargs)

    def send_json(self, status, data):
        payload = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def do_POST(self):
        if self.path != "/simulate":
            self.send_json(404, {"error": "Endpoint not found."})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 1 or length > 100_000:
                raise ValueError("Invalid request size.")

            data = json.loads(self.rfile.read(length))

            algorithm = data.get("algorithm")
            quantum = data.get("quantum")
            processes = data.get("processes")

            allowed = {"AUTO", "FCFS", "SJF", "SRTF", "PRIORITY", "RR"}

            if algorithm not in allowed:
                raise ValueError("Invalid scheduling algorithm.")

            if type(quantum) is not int or not 1 <= quantum <= 1_000_000:
                raise ValueError("Invalid time quantum.")

            if not isinstance(processes, list) or not 1 <= len(processes) <= 100:
                raise ValueError("Submit between 1 and 100 processes.")

            lines = [f"{algorithm} {quantum} {len(processes)}"]
            seen = set()

            for p in processes:
                pid = p.get("pid")
                arrival = p.get("arrival")
                burst = p.get("burst")
                priority = p.get("priority")

                if not isinstance(pid, str) or not re.fullmatch(
                    r"[A-Za-z0-9_-]{1,12}", pid
                ):
                    raise ValueError("Invalid process ID.")

                if pid in seen:
                    raise ValueError(f"Duplicate process ID: {pid}")
                seen.add(pid)

                for value, name, minimum in (
                    (arrival, "arrival time", 0),
                    (burst, "burst time", 1),
                    (priority, "priority", 0),
                ):
                    if (
                        type(value) is not int
                        or not minimum <= value <= 1_000_000
                    ):
                        raise ValueError(f"Invalid {name} for {pid}.")

                lines.append(f"{pid} {arrival} {burst} {priority}")

            if not EXE.is_file():
                self.send_json(
                    500,
                    {"error": "C++ executable not found. Compile scheduler.cpp first."},
                )
                return

            result = subprocess.run(
                [str(EXE)],
                input="\n".join(lines) + "\n",
                capture_output=True,
                text=True,
                timeout=10,
                cwd=BASE_DIR,
                check=False,
            )

            output = result.stdout.strip()

            if not output:
                detail = result.stderr.strip() or "No output from C++ simulator."
                self.send_json(500, {"error": detail[:1000]})
                return

            result_data = json.loads(output)

            if result.returncode != 0 or "error" in result_data:
                self.send_json(400, result_data)
                return

            self.send_json(200, result_data)

        except (ValueError, TypeError, KeyError, json.JSONDecodeError) as e:
            self.send_json(400, {"error": str(e)})
        except subprocess.TimeoutExpired:
            self.send_json(500, {"error": "Simulation timed out."})
        except OSError as e:
            self.send_json(500, {"error": f"Could not run C++ program: {e}"})

    def log_message(self, fmt, *args):
        print(f"[server] {fmt % args}")


if __name__ == "__main__":
    server = ThreadingHTTPServer((HOST, PORT), SimulatorHandler)
    print(f"CPU Scheduling Simulator: http://{HOST}:{PORT}")
    print("Press Ctrl+C to stop the server.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
    finally:
        server.server_close()
