#!/usr/bin/env python3
import queue
import sys
import threading
import tkinter as tk
from dataclasses import dataclass

LANES = ("RUN", "Q0", "Q1", "Q2")
LANE_COLORS = {
    "RUN": ("#14342B", "#A8E6CF"),
    "Q0": ("#173F5F", "#7FDBFF"),
    "Q1": ("#5C3B1E", "#FFC75F"),
    "Q2": ("#5B2333", "#FF8FA3"),
}


@dataclass
class Bubble:
    rect: int
    text: int
    x: float
    y: float
    tx: float
    ty: float
    lane: str


def parse_snapshot(raw_line: str):
    line = raw_line.strip()
    if not line.startswith("MLFQ|"):
        return None

    snapshot = {lane: [] for lane in LANES}

    for segment in line.split("|")[1:]:
        if ":" not in segment:
            continue

        name, payload = segment.split(":", 1)
        lane = name.strip().upper()
        payload = payload.strip()

        if lane not in snapshot:
            continue

        if payload == "" or payload == "-":
            snapshot[lane] = []
            continue

        pids = []
        for item in payload.split(","):
            item = item.strip()
            if not item:
                continue

            try:
                pids.append(int(item))
            except ValueError:
                pass

        snapshot[lane] = pids

    return snapshot


class SerialReader(threading.Thread):
    def __init__(self, output_queue):
        super().__init__(daemon=True)
        self.output_queue = output_queue

    def run(self):
        for raw_line in sys.stdin:
            snapshot = parse_snapshot(raw_line)
            if snapshot is not None:
                self.output_queue.put(snapshot)


class Visualizer:
    def __init__(self, root):
        self.root = root
        self.root.title("MLFQ Scheduler Visualizer")
        self.root.geometry("1180x680")
        self.root.configure(bg="#F5EFD7")

        self.canvas = tk.Canvas(
            root,
            width=1180,
            height=680,
            bg="#F5EFD7",
            highlightthickness=0,
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.status_text = "Waiting for MLFQ telemetry on stdin..."
        self.queue = queue.Queue()
        self.reader = SerialReader(self.queue)
        self.reader.start()

        self.bubbles = {}
        self.lane_top = {}
        self.draw_background()
        self.animate()
        self.consume_updates()

    def draw_background(self):
        self.canvas.create_text(
            590,
            40,
            text="MLFQ Telemetry",
            fill="#2D1E2F",
            font=("Avenir Next", 26, "bold"),
        )
        self.canvas.create_text(
            590,
            76,
            text="Live queue movement from QEMU serial output",
            fill="#61525B",
            font=("Avenir Next", 14),
        )
        self.status_text_id = self.canvas.create_text(
            590,
            645,
            text=self.status_text,
            fill="#5E554B",
            font=("Avenir Next", 13),
        )

        for index, lane in enumerate(LANES):
            top = 120 + index * 120
            self.lane_top[lane] = top
            bg, accent = LANE_COLORS[lane]

            self.canvas.create_rectangle(
                60,
                top,
                1120,
                top + 88,
                fill=bg,
                outline="",
            )
            self.canvas.create_rectangle(
                60,
                top,
                160,
                top + 88,
                fill=accent,
                outline="",
            )
            self.canvas.create_text(
                110,
                top + 44,
                text=lane,
                fill="#1E1E1E",
                font=("Avenir Next", 18, "bold"),
            )

    def lane_position(self, lane, slot):
        top = self.lane_top[lane]
        x = 210 + slot * 112
        y = top + 44
        return x, y

    def create_bubble(self, pid, lane, slot):
        x, y = self.lane_position(lane, slot)
        rect = self.canvas.create_oval(
            x - 28,
            y - 28,
            x + 28,
            y + 28,
            fill="#FFF8DC",
            outline="#2D1E2F",
            width=2,
        )
        text = self.canvas.create_text(
            x,
            y,
            text=str(pid),
            fill="#2D1E2F",
            font=("Avenir Next", 14, "bold"),
        )
        self.bubbles[pid] = Bubble(rect, text, x, y, x, y, lane)

    def set_targets(self, snapshot):
        positions = {}
        for lane in LANES:
            for slot, pid in enumerate(snapshot.get(lane, [])):
                positions[pid] = (lane, slot)

        current_pids = set(self.bubbles)
        target_pids = set(positions)

        for pid in sorted(target_pids - current_pids):
            lane, slot = positions[pid]
            self.create_bubble(pid, lane, slot)

        for pid in current_pids - target_pids:
            bubble = self.bubbles.pop(pid)
            self.canvas.delete(bubble.rect)
            self.canvas.delete(bubble.text)

        for pid, (lane, slot) in positions.items():
            bubble = self.bubbles[pid]
            bubble.tx, bubble.ty = self.lane_position(lane, slot)
            bubble.lane = lane

    def consume_updates(self):
        last_snapshot = None

        while True:
            try:
                last_snapshot = self.queue.get_nowait()
            except queue.Empty:
                break

        if last_snapshot is not None:
            self.set_targets(last_snapshot)
            self.status_text = (
                "Streaming... darker lanes are lower priority, so long-lived CPU work drifts down."
            )
            self.canvas.itemconfig(self.status_text_id, text=self.status_text)

        self.root.after(40, self.consume_updates)

    def animate(self):
        for bubble in self.bubbles.values():
            bubble.x += (bubble.tx - bubble.x) * 0.22
            bubble.y += (bubble.ty - bubble.y) * 0.22

            self.canvas.coords(
                bubble.rect,
                bubble.x - 28,
                bubble.y - 28,
                bubble.x + 28,
                bubble.y + 28,
            )
            self.canvas.coords(bubble.text, bubble.x, bubble.y)

        self.root.after(16, self.animate)


def main():
    root = tk.Tk()
    Visualizer(root)
    root.mainloop()


if __name__ == "__main__":
    main()
