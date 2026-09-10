# Repository instructions

- Keep this repository concise and safety-first; assume the reader is new to electronics.
- Never guess hardware facts. Ask the user.
- Never read vendored docs unless asked specifically or adding new components (then `uv run pypdf`, convert it to markdown into temporary place and read grep. NEVER read whole file at once, it causes instant context overflow). The agent that adds new component is responsible for vendoring 3rdparty docs and putting relevant TLDR; to hardware.md
- Cross-check every electrical limit, pin, and connection before changing `wiring/car.yml`; 
- After wiring changes, run `uv run wireviz wiring/car.yml` and inspect the rendered diagram.
