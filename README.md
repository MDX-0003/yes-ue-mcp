# yes-ue-mcp

**Native C++ Model Context Protocol (MCP) plugin for Unreal Engine 5.6+**

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

The plugin starts an HTTP server on `localhost:8080/mcp`. Configure in `Config/DefaultYesUeMcp.ini`:

```ini
[/Script/YesUeMcpEditor.McpServerSettings]
; HTTP server port (default: 8080)
; Change this if running multiple UE instances
ServerPort=8080

; Auto-start server when editor opens
bAutoStartServer=true

; Bind address (default: localhost for security)
BindAddress=127.0.0.1
```

> **Tip:** When running multiple UE projects, set a different `ServerPort` for each project (e.g., 8080, 8081, 8082).

### Client Configuration

**Claude Code CLI**:
```bash
claude mcp add --transport http unreal-engine http://localhost:8080/mcp
```

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

## Available Tools

### Blueprint Analysis (7 tools)

#### `analyze-blueprint`
Complete Blueprint structure analysis including parent class, functions, variables, and components.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path (e.g., `/Game/Blueprints/BP_Character`)

**Returns:** JSON with complete Blueprint metadata

---

#### `get-blueprint-functions`
List all functions with signatures, parameters, return types, and metadata.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `function_filter` (string, optional) - Filter by function name (wildcards supported)

**Returns:** Array of function definitions with full signatures

---

#### `get-blueprint-variables`
List all variables with types, default values, replication settings, and metadata.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `variable_filter` (string, optional) - Filter by variable name (wildcards supported)

**Returns:** Array of variables with types and default values

---

#### `get-blueprint-components`
Get component hierarchy with transforms and attachment relationships.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `include_transforms` (boolean, optional) - Include component transforms (default: true)

**Returns:** Component tree with hierarchy and transforms

---

#### `get-blueprint-graph`
Read complete Blueprint graph structure including all nodes, connections, and pin data.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `graph_name` (string, optional) - Specific graph name
- `graph_type` (string, optional) - Filter: `event`, `function`, or `macro`
- `include_positions` (boolean, optional) - Include node X/Y positions (default: false)

**Returns:** All graphs with nodes, pins, and connections

---

#### `get-blueprint-node`
Get detailed information about a specific Blueprint node by GUID.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `node_guid` (string, required) - Node GUID to inspect

**Returns:** Full node details with pins and connections

---

#### `get-blueprint-defaults`
Read CDO (Class Default Object) property values from Blueprint.

**Parameters:**
- `asset_path` (string, required) - Blueprint asset path
- `property_filter` (string, optional) - Filter by property name (wildcards supported)
- `category_filter` (string, optional) - Filter by property category

**Returns:** All property defaults with types, categories, and flags

---

### Level/World Tools (2 tools)

#### `query-level`
List actors in the currently open level with filtering options.

**Parameters:**
- `class_filter` (string, optional) - Filter by actor class (wildcards supported)
- `folder_filter` (string, optional) - Filter by World Outliner folder path
- `tag_filter` (string, optional) - Filter by actor tag
- `include_hidden` (boolean, optional) - Include hidden actors (default: false)
- `include_components` (boolean, optional) - Include component list (default: false)
- `include_transform` (boolean, optional) - Include transforms (default: true)
- `limit` (integer, optional) - Max results (default: 100)

**Returns:** Array of actors with optional transforms and components

---

#### `get-actor-details`
Deep inspection of a specific actor in the level.

**Parameters:**
- `actor_name` (string, required) - Actor name or label to inspect
- `include_properties` (boolean, optional) - Include all properties (default: true)
- `include_components` (boolean, optional) - Include component details (default: true)

**Returns:** Complete actor details with properties and component hierarchy

---

### Project Configuration (1 tool)

#### `get-project-settings`
Query project configuration settings.

**Parameters:**
- `section` (string, optional) - Section to query: `input`, `collision`, `tags`, `maps`, or `all` (default: `all`)

**Returns:** JSON with requested configuration sections:
- **input**: Action/axis mappings with keys and modifiers
- **collision**: Profiles, channels, and responses
- **tags**: Gameplay tag sources and settings
- **maps**: Default maps and game modes

---

### Analysis Tools (2 tools)

#### `get-class-hierarchy`
Browse class inheritance tree showing parents and children.

**Parameters:**
- `class_name` (string, required) - Class to inspect (e.g., `AActor`, `UActorComponent`)
- `direction` (string, optional) - `parents`, `children`, or `both` (default: `both`)
- `include_blueprints` (boolean, optional) - Include Blueprint subclasses (default: true)
- `depth` (integer, optional) - Max inheritance depth (default: 10)

**Returns:** Class hierarchy with parent chain and child classes

---

#### `inspect-data-asset`
Read DataTable and DataAsset contents.

**Parameters:**
- `asset_path` (string, required) - DataTable or DataAsset path
- `row_filter` (string, optional) - Filter rows by name (wildcards supported, DataTable only)

**Returns:**
- **DataTable**: All rows with field data
- **DataAsset**: All properties with values

---

### Asset Management (1 tool)

#### `search-assets`
Search assets by name, class, or path with wildcard support.

**Parameters:**
- `pattern` (string, optional) - Search pattern (wildcards supported)
- `class_filter` (string, optional) - Filter by asset class
- `path_filter` (string, optional) - Filter by asset path
- `limit` (integer, optional) - Max results (default: 100)

**Returns:** Array of matching assets with paths and types

## Architecture

```
┌──────────────────┐  HTTP POST   ┌──────────────────────────────┐
│  Claude / Cursor │ ──────────►  │  UE Editor                   │
│  Windsurf / etc  │  JSON-RPC    │  └─ YesUeMcp Plugin          │
│                  │ ◄──────────  │     └─ FHttpServerModule     │
│                  │   Response   │        (localhost:8080/mcp)│
└──────────────────┘              └──────────────────────────────┘
```

**Modules:**
- `YesUeMcp` (Runtime) - Core MCP protocol layer (JSON-RPC, tool registry)
- `YesUeMcpEditor` (Editor) - HTTP server + UE Editor tool implementations

## Development

### Adding Custom Tools

See [Docs/ToolDevelopment.md](Docs/ToolDevelopment.md) for a guide on creating custom MCP tools.

### Building

Requires Unreal Engine 5.6 or higher.

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
