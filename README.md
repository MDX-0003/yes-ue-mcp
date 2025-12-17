# yes-ue-mcp

**Native C++ Model Context Protocol (MCP) plugin for Unreal Engine 5.4+**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Overview

yes-ue-mcp is a native C++ Unreal Engine plugin that implements the [Model Context Protocol (MCP)](https://modelcontextprotocol.io) over Streamable HTTP. It enables AI assistants like Claude, Cursor, Windsurf, and others to interact directly with the Unreal Editor.

**Key Features:**
- ✅ **Zero Dependencies** - Uses UE's built-in `FHttpServerModule`
- ✅ **Cross-LLM Compatible** - Works with Claude, Cursor, Windsurf, VS Code Copilot, Continue, OpenAI, and more
- ✅ **Editor-Integrated** - HTTP server runs directly in UE Editor, no separate process needed
- ✅ **Extensible** - Easy-to-use tool registration system for custom MCP tools

## Quick Start

### Installation

1. Clone into your project's `Plugins` directory:
```bash
cd YourProject/Plugins
git clone https://github.com/softdaddy-o/yes-ue-mcp.git
```

2. Regenerate project files and build your project

3. Enable the plugin in your `.uproject` file or via Editor → Plugins

### Configuration

The plugin starts an HTTP server on `localhost:8080/mcp` by default. Configure in `Config/DefaultYesUeMcp.ini`:

```ini
[/Script/YesUeMcpEditor.McpServerSettings]
; HTTP server port (default: 8080)
ServerPort=8080

; Auto-start server when editor opens
bAutoStartServer=true

; Bind address (default: localhost for security)
BindAddress=127.0.0.1
```

### Client Configuration

**Claude Desktop** (`claude_desktop_config.json`):
```json
{
  "mcpServers": {
    "unreal-engine": {
      "url": "http://localhost:8080/mcp"
    }
  }
}
```

**Cursor** (`.cursor/mcp.json`):
```json
{
  "mcpServers": {
    "unreal-engine": {
      "url": "http://localhost:8080/mcp"
    }
  }
}
```

**VS Code Continue** (`.continue/config.json`):
```json
{
  "mcpServers": [{
    "name": "unreal-engine",
    "transport": { "type": "http", "url": "http://localhost:8080/mcp" }
  }]
}
```

## Available Tools (v1.0 - Read-Only)

### Blueprint Analysis
- `analyze-blueprint` - Complete Blueprint structure analysis
- `get-blueprint-functions` - List all functions with signatures
- `get-blueprint-variables` - List variables with types and defaults
- `get-blueprint-components` - Get component hierarchy
- `get-blueprint-event-graph` - Detailed event graph node information
- `find-blueprint-references` - Find where a Blueprint is used

### Asset Management
- `search-assets` - Search assets by name/type/path
- `get-asset-references` - Get asset dependency graph

### Level/Actor Tools
- `list-level-actors` - List all actors in current level
- `find-actors-by-class` - Filter actors by class type
- `get-actor-properties` - Read actor property values

## Architecture

```
┌──────────────────┐  HTTP POST   ┌─────────────────────────────┐
│  Claude / Cursor │ ──────────►  │  UE Editor                  │
│  Windsurf / etc  │  JSON-RPC    │  └─ YesUeMcp Plugin         │
│                  │ ◄──────────  │     └─ FHttpServerModule    │
│                  │   Response   │        (localhost:8080/mcp) │
└──────────────────┘              └─────────────────────────────┘
```

**Modules:**
- `YesUeMcp` (Runtime) - Core MCP protocol layer (JSON-RPC, tool registry)
- `YesUeMcpEditor` (Editor) - HTTP server + UE Editor tool implementations

## Development

### Adding Custom Tools

See [Docs/ToolDevelopment.md](Docs/ToolDevelopment.md) for a guide on creating custom MCP tools.

### Building

Requires Unreal Engine 5.4 or higher.

```bash
# Build with UnrealBuildTool
<UE>/Engine/Build/BatchFiles/Build.bat YourProjectEditor Win64 Development
```

## License

MIT License - see [LICENSE](LICENSE) for details

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Links

- [GitHub Repository](https://github.com/softdaddy-o/yes-ue-mcp)
- [Model Context Protocol](https://modelcontextprotocol.io)
- [MCP Specification](https://modelcontextprotocol.io/specification)
