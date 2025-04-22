# SwiftNet - High-Performance Networking Library

## ⚠️ This is a pre-1.0 release. The library is subject to change. Use at your own risk and report issues!

### SwiftNet is a simple and easy-to-use networking library built using raw sockets. It is designed for developers who value simplicity, readability, and good performance.

## Features
- **💡 Ease of Use**: Simple API designed to get up and running quickly, without needing to deal directly with raw sockets.
- **🚀 High Performance**: Optimized for speed while maintaining readability.
- **📂 Lightweight**: Minimal dependencies and a small footprint.

## Why Use SwiftNet?
- **Straightforward API:** Get up and running with minimal setup.
- **Solid Performance:** Efficient handling of networking tasks.
- **Open Source and Collaborative:** Contributions are welcome to make it even better.

## Installation
Follow these steps to install SwiftNet:
1. Clone the repository to your local machine:
```bash
git clone https://github.com/deadlightreal/SwiftNet
```
2. Navigate to the build directory inside the SwiftNet directory:
```bash
cd SwiftNet/build
```
3. Generate the build files using CMake:
```bash
cmake ../src
```
4. Build the project using Make:
```bash
make
```
5. To use SwiftNet in your project:
- Include the SwiftNet.h header from the `src` directory in your main source file (e.g., `main.c`).
- Link against the static library (e.g., `libswift_net_server.a` or `libswift_net_client.a`) using your compiler.
- Add `-DSWIFT_NET_CLIENT` or `-DSWIFT_NET_SERVER` to your build files.
- Define `SWIFT_NET_CLIENT` or `SWIFT_NET_SERVER` in your code for better autocompletion.

## Contributing
### Contributions are very welcome! If you'd like to improve the library or fix any issues, feel free to fork the repository and submit a pull request.

## Code Contributions
We have simple guidelines for contributing:

- Write clear and concise comments for your code.
- Ensure that the code is easy to read and follows the existing style.

We value collaboration and clean code!

## License
This project is licensed under the Apache License 2.0

## Contact
For any questions or support, feel free to open an issue or contact me at [richardfabianmain@gmail.com].
