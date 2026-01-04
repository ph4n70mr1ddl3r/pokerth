Best practice is to use the VS Code Dev-Container feature.

Before building the container Image, edit Dockerfile in `.devcontainer` folder and set architecture and target to build for.
... you might also need to edit docker-compose.yml for network settings

Inside the running container:

`cd ${ROOT}/pokerth`
`bash docker/windows/build_windows.sh`

The `pokerth_client.exe` plus all necessary dlls are located in `${ROOT}/pokerth/build/deploy`.
You can zip the whole deploy folder and transfer it to your windows machine.
... tested on Windows 11


