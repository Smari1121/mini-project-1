# C-Shell (Custom C Shell)

Hey there! This is a custom shell written in C from scratch, complete with its own lexer and parser. It handles custom built-ins, complex pipelines, and I/O redirection.

## How to Run

1. Navigate to the `c-shell` directory:
   ```bash
   cd c-shell
   ```
2. Build the project using `make`:
   ```bash
   make
   ```
3. Run the compiled binary:
   ```bash
   ./shell.out
   ```

## Assumptions & Design Choices

Here are a few assumptions and specific design decisions made during development:

- **Built-in Commands in Pipelines:** The intrinsic commands (`hop`, `reveal`, `peek`, `locate`) are treated as first-class citizens. This means you can pipe to and from them just like external binaries! If a built-in is run as a single command, it executes in the main shell process (so `hop` actually changes your directory). If it's part of a pipeline, it forks into a subshell to avoid messing with the parent shell's state unexpectedly.
- **Frecency Storage:** The `hop` command's frecency database is persistent across sessions. It saves a tiny footprint file at `~/.cshell_frecency`.
- **Parsing & Execution:** We built a custom lexer and parser from the ground up rather than relying on `system()` or `popen()`. This handles tricky quote escaping and string concatenation perfectly (e.g., `echo "hello"world` is parsed as one word).
- **Redirection Pre-flights:** Before executing a command block, the shell runs a "pre-flight" check on all specified `<` and `>` files. If an output file can't be created or an input file doesn't exist, the shell safely aborts without running the command.
- **Multiple Redirections:** You can totally do things like `cat < a.txt < b.txt`. The shell handles this under the hood by spinning up a lightweight "feeder" or "writer" process that concatenates the streams seamlessly.
- **Limits:** Hardcoded buffer sizes (like max 256 arguments per command or `PATH_MAX` limitations) were used to keep memory management straightforward and fast without arbitrary dynamic allocations everywhere.
- **Robust Pipelines:** Infinite `N`-stage pipelines are supported using `pipe()`. The shell rigorously closes all unused pipe file descriptors in both parent and child processes to ensure correct `EOF` signal propagation without hanging zombie processes.

Enjoy exploring the shell!
