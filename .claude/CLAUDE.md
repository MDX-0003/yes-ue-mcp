# yes-ue-mcp - Claude Instructions

**Native C++ MCP Plugin for Unreal Engine 5.6+**

## Project Overview

This plugin implements the Model Context Protocol (MCP) over HTTP, allowing AI assistants (Claude Code, Cursor, Windsurf, etc.) to inspect, analyze, and **modify** Unreal Engine projects through a standardized JSON-RPC API.

## GitHub Repositories

| Remote | Repository URL | Purpose |
|--------|----------------|---------|
| `origin` | https://github.com/softdaddy-o/yes-ue-mcp-private.git | Development (private) |
| `public` | https://github.com/softdaddy-o/yes-ue-mcp.git | Release (public) |

## Workflow Rules

### Issue-Driven Development
- **Every task MUST have a GitHub issue** - Create an issue before starting any task
- **Close the issue** when the task is complete with a commit referencing it (e.g., `Closes #123`)
- Use conventional commit format: `feat:`, `fix:`, `docs:`, `refactor:`

### Git Workflow

**Private Repository (origin)**
- Commit frequently - whenever one issue/task is finished
- Each commit should reference its issue number
- Push to `origin` for development work

```bash
git add -A && git commit -m "feat: add new tool

Closes #123"
git push origin main
```

**Public Repository (public)**
- Push aggregated changes only when a new version is ready
- Use version tags (v1.0.0, v1.1.0, etc.)
- Filter out `.claude/` directory when pushing to public

```bash
# Push to public with .claude/ filtered out
git push public main --force-with-lease
git tag v1.0.0
git push public --tags
```

**Publishing to Public Repo**
When pushing to the public repository, use git filter to exclude private files:
```bash
# Create a filtered branch for public release
git checkout -b release-temp
git filter-branch --tree-filter 'rm -rf .claude' HEAD
git push public release-temp:main --force
git checkout main
git branch -D release-temp
```

## Testing Environment

**Primary Test Project:** Elpis (Action RPG - UE 5.7)
- **Project Path:** `F:\src3\Covenant\ElpisClient\`
- **Plugin Install Path:** `F:\src3\Covenant\ElpisClient\Plugins\yes-ue-mcp\`
- **Version Control:** Perforce (plugin excluded via `.p4ignore`)
- **MCP Endpoint:** `http://127.0.0.1:8080/mcp`
- **Status:** Primary test environment

**Secondary Test Project:** GameAnimationSample56 (UE 5.6)
- **Project Path:** `F:\src_ue5\GameAnimationSample56\`
- **Plugin Install Path:** `F:\src_ue5\GameAnimationSample56\Plugins\yes-ue-mcp\`
- **MCP Endpoint:** `http://127.0.0.1:8081/mcp`
- **Status:** Active test environment for UE 5.6

### Deploying to Test Projects

Use `copy_plugin.ps1` to safely copy the plugin to test projects:

```powershell
# Copy to both projects (default)
.\copy_plugin.ps1

# Copy to Elpis only
.\copy_plugin.ps1 -Target Elpis

# Copy to GameAnimationSample56 only
.\copy_plugin.ps1 -Target GameAnim
```

**Excludes:** `.git`, `.claude`, `Tests`, `.pytest_cache`, `.github`, `Docs`, test files

**Config Override:** Target projects can override settings (e.g., `ServerPort`) via their own `Config/DefaultYesUeMcp.ini`

## Module Structure

- **YesUeMcp** (Runtime) - Core MCP protocol layer
- **YesUeMcpEditor** (Editor) - HTTP server + tool implementations

## Coding Standards

- Follow Epic's C++ coding conventions
- Use `YESUEMCP_API` / `YESUEMCPEDITOR_API` for exported symbols
- All tools inherit from `UMcpToolBase`
- Register tools in `FYesUeMcpEditorModule::RegisterBuiltInTools()`

## Version Management

**Version is defined in TWO places (keep in sync):**
1. `YesUeMcp.uplugin` - `VersionName` field (read by UE plugin system)
2. `Source/YesUeMcp/Public/YesUeMcp.h` - `YESUEMCP_VERSION` macro (compile-time constant)

**IMPORTANT: Always increment version when modifying code!**

- Use semantic versioning: `MAJOR.MINOR.PATCH`
  - **MAJOR**: Breaking changes to existing tools or protocol
  - **MINOR**: New features (new tools, new parameters)
  - **PATCH**: Bug fixes, internal improvements, documentation

**When to increment:**
- Adding a new tool → MINOR
- Adding new parameter to existing tool → MINOR
- Changing tool input/output schema → MINOR (or MAJOR if breaking)
- Bug fixes → PATCH
- Internal refactoring → PATCH
- Any code change that affects behavior → PATCH minimum

## Key Files

- `Source/YesUeMcp/Public/Tools/McpToolBase.h` - Base class for all tools
- `Source/YesUeMcp/Public/Tools/McpToolRegistry.h` - Tool registration
- `Source/YesUeMcpEditor/Public/Server/McpServer.h` - HTTP server
- `Source/YesUeMcpEditor/Public/Subsystem/McpEditorSubsystem.h` - Lifecycle management
- `Source/YesUeMcpEditor/Public/Utils/McpAssetModifier.h` - Write operation utilities

## MCP Server

- **Protocol:** MCP 2025-03-26 (Streamable HTTP) with JSON-RPC 2.0
- **Transport:** HTTP (Streamable HTTP)
- **Endpoint:** `/mcp`
- **Port:** 8080 (configurable)
- **CORS:** Enabled for cross-origin requests

## Available Tools (30 total)

### Read Tools (10) - Consolidated in v1.6.0

#### Blueprint Tools
| Tool | Description |
|------|-------------|
| `query-blueprint` | Query Blueprint structure: functions, variables, components, defaults. Use `include` param to select what to return. (Merged: analyze-blueprint, get-blueprint-functions, get-blueprint-variables, get-blueprint-components, get-blueprint-defaults) |
| `query-blueprint-graph` | Query Blueprint graphs: event graphs, functions, macros, nodes. Use `node_guid` for specific node, `callable_name` for callable details, `list_callables` for lightweight list. (Merged: get-blueprint-graph, get-blueprint-node, list-blueprint-callables, get-callable-details) |

#### Asset Tools
| Tool | Description |
|------|-------------|
| `query-asset` | Search or inspect assets. Use `query` param for search mode, `asset_path` for inspect mode. Handles DataTables and DataAssets. (Merged: search-assets, inspect-asset, inspect-data-asset) |

#### Material Tools
| Tool | Description |
|------|-------------|
| `query-material` | Query Material expression graph and parameters. Use `include` param: 'graph', 'parameters', or 'all'. (Merged: get-material-graph, get-material-parameters) |

#### Level Tools
| Tool | Description |
|------|-------------|
| `query-level` | List actors with filtering, or get detailed info for a specific actor (use `actor_name` for detail mode) |

#### Project Tools
| Tool | Description |
|------|-------------|
| `get-project-info` | Get project/plugin info, optionally include settings (use `section` param for input/collision/tags/maps) |
| `get-class-hierarchy` | Browse class inheritance (parents/children) |

#### Reference Tools
| Tool | Description |
|------|-------------|
| `find-references` | Find references to assets, Blueprint variables, or nodes (type: asset/property/node) |

#### Widget Tools
| Tool | Description |
|------|-------------|
| `inspect-widget-blueprint` | Inspect Widget Blueprint hierarchy, slots (anchors, offsets, sizes), visibility, property bindings, and animations |

#### Debug Tools
| Tool | Description |
|------|-------------|
| `get-logs` | Retrieve UE Output Log entries with filtering (category, severity, search) |

### Write Tools (20)

#### Property Tools
| Tool | Description |
|------|-------------|
| `set-property` | Set any property on any asset using UE reflection (supports nested paths, arrays, structs) |
| `compile-blueprint` | Compile a Blueprint or AnimBlueprint after modifications |
| `save-asset` | Save a modified asset to disk |

#### Graph Tools
| Tool | Description |
|------|-------------|
| `add-graph-node` | Add a node to Blueprint or Material graph |
| `remove-graph-node` | Remove a node from any graph |
| `connect-graph-pins` | Connect two pins in any graph |
| `disconnect-graph-pin` | Break pin connections |

#### Asset Creation Tools
| Tool | Description |
|------|-------------|
| `create-asset` | Create new asset (Blueprint, Material, DataTable, Level, etc.) |
| `delete-asset` | Delete an asset from the project |

#### Level Editing Tools
| Tool | Description |
|------|-------------|
| `spawn-actor` | Spawn an actor in the current level (native or Blueprint class) |
| `delete-actor` | Delete an actor from the level |
| `add-component` | Add a component to an existing actor |
| `remove-component` | Remove a component from an actor |

#### Widget Tools
| Tool | Description |
|------|-------------|
| `add-widget` | Add a widget to a WidgetBlueprint tree |
| `remove-widget` | Remove a widget from a WidgetBlueprint |

#### DataTable Tools
| Tool | Description |
|------|-------------|
| `add-datatable-row` | Add a row to a DataTable |
| `remove-datatable-row` | Remove a row from a DataTable |

#### Scripting Tools
| Tool | Description |
|------|-------------|
| `run-python-script` | Execute Python scripts in Unreal Editor (inline or file). Supports argument passing via `arguments` param. Requires PythonScriptPlugin enabled. |

#### Build Tools
| Tool | Description |
|------|-------------|
| `trigger-live-coding` | Trigger Live Coding compilation for C++ code changes (Ctrl+Alt+F11 equivalent). Windows only. Requires Live Coding enabled in Editor Preferences. |

## Write Tool Usage

### Property Modification Pattern
```json
// Set a simple property
{ "asset_path": "/Game/BP_Player", "property_path": "MaxHealth", "value": 100 }

// Nested property
{ "asset_path": "/Game/BP_Player", "property_path": "Stats.Damage", "value": 25 }

// Array element
{ "asset_path": "/Game/BP_Player", "property_path": "Items[0].Count", "value": 5 }

// Vector as array
{ "asset_path": "/Game/BP_Actor", "property_path": "Location", "value": [100, 200, 0] }
```

### Asset Modification Workflow
1. Use `set-property` to modify values
2. Use `compile-blueprint` if modifying a Blueprint
3. Use `save-asset` to persist changes

### Transaction Support
All write operations are wrapped in `FScopedTransaction` for undo/redo support.

### Python Scripting Pattern
```json
// Inline script
{
  "script": "import unreal\nprint('Hello from Python')"
}

// Script file
{
  "script_path": "/path/to/script.py"
}

// With arguments
{
  "script": "import unreal\nargs = unreal.get_mcp_args()\nprint(args.get('name'))",
  "arguments": {"name": "MyAsset", "count": 42}
}
```

**Python Script Workflow:**
1. Use `run-python-script` with inline code or file path
2. Access arguments via `unreal.get_mcp_args()` if provided
3. Use full `unreal` module API for Editor operations
4. Output is captured and returned in response

**Requirements:**
- PythonScriptPlugin must be enabled in Unreal Editor
- Enable via: Edit > Plugins > Scripting > Python Editor Script Plugin
- Restart editor after enabling

## Logging

All MCP tools log to the `LogYesUeMcp` category. Use `get-logs` with `category="LogYesUeMcp"` to debug tool execution.

## Future Work

See GitHub Issues for planned features and enhancements.

### Potential Additions
- Animation asset query (AnimBP, montages, notifies)
- AI behavior tree analysis
- Streaming support for large datasets
- MCP Resources (expose project files)
- MCP Prompts (template code generation)

## Known Limitations

1. **Editor-only** - Cannot run in packaged game
2. **Single session** - No multi-client support
3. **No streaming** - Synchronous responses only

## Project History

Historical development notes have been migrated to GitHub Issues for reference.
See: https://github.com/softdaddy-o/yes-ue-mcp-private/issues?q=label%3Adocumentation
