#!/usr/bin/env python3

import subprocess
import sys
import threading
from pathlib import Path


def read_output(process, prefix):
    """Read output from a process and print it with the given prefix"""
    try:
        while True:
            line = process.stdout.readline()
            if not line:
                break
            # Decode bytes to string and strip newline
            line = line.decode("utf-8").rstrip("\n")
            if line:
                print(f"{prefix} {line}")
    except Exception as e:
        print(f"{prefix} Error reading output: {e}")


def main():
    # Get number of agents from command line, default to 2
    num_agents = 2
    if len(sys.argv) > 1:
        try:
            num_agents = int(sys.argv[1])
        except ValueError:
            print(f"Usage: {sys.argv[0]} [num_agents]")
            sys.exit(1)

    # Get the base directory
    base_dir = Path(__file__).parent
    agent_exe = base_dir / "build" / "agent"
    manager_exe = base_dir / "build" / "manager"

    # Check if executables exist
    if not agent_exe.exists():
        print(f"Error: Agent executable not found at {agent_exe}")
        sys.exit(1)
    if not manager_exe.exists():
        print(f"Error: Manager executable not found at {manager_exe}")
        sys.exit(1)

    processes = []
    threads = []

    try:
        # Start N agents on different ports
        base_port = 54001
        agent_addrs = []
        for i in range(num_agents):
            port = base_port + i
            print(f"Starting agent {i} on port {port}...")
            process = subprocess.Popen(
                [str(agent_exe), str(port)],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=False,
            )
            processes.append(process)

            # Create thread to read output
            prefix = f"agent {i} |"
            thread = threading.Thread(
                target=read_output, args=(process, prefix), daemon=True
            )
            thread.start()
            threads.append(thread)

            agent_addrs.append(f"127.0.0.1:{port}")

        # Start manager
        print("Starting manager...")
        manager_process = subprocess.Popen(
            [str(manager_exe)] + agent_addrs,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=False,
        )
        processes.append(manager_process)

        # Create thread to read manager output
        prefix = "manager |"
        thread = threading.Thread(
            target=read_output, args=(manager_process, prefix), daemon=True
        )
        thread.start()
        threads.append(thread)

        print(f"\n{num_agents} agents and 1 manager started. Press Ctrl+C to stop.\n")

        # Wait for all processes to finish
        for process in processes:
            process.wait()

    except KeyboardInterrupt:
        print("\nShutting down processes...")
        for process in processes:
            try:
                process.terminate()
                # Give it a moment to terminate gracefully
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
            except Exception as e:
                print(f"Error terminating process: {e}")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
