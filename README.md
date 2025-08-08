# C-Gemini Chatbot: A C-Based Chat Application with Gemini Pro API Integration

 
*A lightweight and efficient chatbot built in C, leveraging the power of the Gemini Pro API for natural language processing and conversational AI.*

---

## Table of Contents
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [API](#api)
- [Contributing](#contributing)
- [License](#license)
- [Support](#support)

---

## Features
- **Lightweight and Fast**: Built in C for optimal performance.
- **Gemini Pro API Integration**: Leverages the Gemini Pro API for advanced conversational AI.
- **Cross-Platform**: Works on Linux, macOS, and Windows (with minor adjustments).
- **Customizable**: Easily modify the chatbot's behavior and responses.
- **History Storage**: Supports up to 40KB of conversation history.
- **Simple Frontend**: Includes a web interface for easy interaction.
- **Runtime Settings**: Configure model, temperature, top-p, top-k, max tokens, and system prompt via REST.
- **Health & Maintenance**: Health check and clear history endpoints.

---

## Prerequisites
To run this application, you'll need the following:
- **C Compiler**: GCC recommended.
- **libcurl**: For making HTTP requests to the Gemini Pro API.
- **json-c**: For parsing JSON responses from the API.
- **Python 3**: For running the frontend server (optional; you can also serve via the C server).

---

## Installation
### On Debian-based systems:
```bash
sudo apt-get update
sudo apt-get install gcc libcurl4-openssl-dev libjson-c-dev libmicrohttpd-dev python3
```

### On Red Hat-based systems:
```bash
sudo yum update
sudo yum install gcc libcurl-devel json-c-devel libmicrohttpd-devel python3
```

---

## Configuration
1. Set your Gemini API key as an environment variable before running the server:
   ```bash
   export GEMINI_API_KEY="your_gemini_pro_api_key_here"
   ```
2. Optional runtime settings can be adjusted via the `/config` endpoint or the frontend Settings modal.

---

## Usage
### Backend
1. Navigate to the `backend` directory:
   ```bash
   cd backend
   ```
2. Build the application:
   ```bash
   make
   ```
3. Run the server:
   ```bash
   ./server
   ```

### Frontend
Open `frontend/index.html` directly or let the C server serve it at `http://localhost:8080/`.

---

## API
- `POST /chat`
  - Body: `{ "message": "..." }`
  - Response: `{ "response": "..." }`
- `GET /config`
  - Returns current runtime settings: `{ model, temperature, top_p, top_k, max_output_tokens, system_prompt }`
- `POST /config`
  - Body (any subset): `{ model, temperature, top_p, top_k, max_output_tokens, system_prompt }`
  - Sets runtime settings.
- `POST /clear`
  - Clears chat history.
- `GET /health`
  - Returns `{ "status": "ok" }`.

---

## Contributing
Contributions are welcome! Here's how you can help:
1. Fork the repository.
2. Create a new branch for your feature or bugfix.
3. Submit a pull request with a detailed description of your changes.

Please ensure your code follows the existing style and includes appropriate documentation.



---

## Support
If you find this project useful, please consider giving it a ⭐️ on GitHub! For questions or feedback, open an issue or reach out to [your email or social media handle].

---

## FAQ
### How do I get a Gemini Pro API key?
Visit the [Gemini Pro website](https://aistudio.google.com/) to sign up and obtain an API key.

### Can I use this on Windows?
Yes, but you may need to adjust the build process. Contributions for Windows support are welcome!

---

## Acknowledgments
- [Gemini Pro API](https://aistudio.google.com) for providing the conversational AI backend.
- [libcurl](https://curl.se/libcurl/) and [json-c](https://github.com/json-c/json-c) for enabling HTTP requests and JSON parsing in C.
- [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) for the embedded HTTP server.