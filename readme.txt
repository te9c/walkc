### walkc - simple educational implementation of a container runtime for linux.

TODO:
1. CLI
    1.1 [x] create
    1.2 [x] delete
    1.3 [x] run
    1.4 [x] start
    1.5 [x] kill
    1.6 [x] spec
    1.7 [x] state
    1.8 [x] help
2. Runtime features
    2.1 [x] pivot_root and namespace isolation
    2.2 [ ] Capabilities isolation
    2.3 [/] Mounts
    2.4 [ ] User control
    2.5 [ ] Cgroups and resource control
3. Etc
    3.1 [ ] IPC for peeking at the state of running container
    3.2 [ ] Handling of pseudo tty