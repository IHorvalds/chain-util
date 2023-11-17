# Chainmail

At the end of a process or multiple processes, run another one.

I am dumb and sometimes forget what commands I need to run after a long running command.
Or some compilation takes too damn long and I want to sleep, but God knows how long it actually takes.

*This helps me with that.*

I'm sure there's an easier way to do it.

## Dependencies
- c++ 20
- win32
- [boost program_options](https://www.boost.org/doc/libs/1_81_0/doc/html/program_options.html)
- vcpkg

## Compilation & Instalation
Open the Visual Studio Solution, select the "Release" configuration and do Ctrl-Shift-B.
If no errors come up, there should be an x64/Release directory next to the .sln file.
In there, there should be the Chainmail.exe, which you can then move anywhere you want.
Add that path to the PATH environment variable if you want it to be accessible easily.

## Usage
```
.\Chainmail.exe [[--pids list-of-pids] | [--procs list-of-proc-name-or-regex]] [--delay number-of-seconds] -n [command or executable to run after all waited processes exit]
```
- `-h [ --help ]`               Show this help message
- `--pids [ --ps ] [list]`      List of PIDs to wait for (optional. Has precedence over `procs`)
- `--procs [list]`               List of names of processes to wait for (optional. Supports ECMAScript style regular expressions)
- `--delay [ number of seconds after processes exit ]`
- `-n [ --next ]`                 Command to run after waited processes exit. Tip: if you need builtin cmd commands, use `-n "cmd /C <command> <args>"`,
 otherwise call executable like you normally would in the cli
