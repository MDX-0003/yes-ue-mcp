# yes-ue-mcp - Claude Instructions

**Native C++ MCP Plugin for Unreal Engine 5.7+**

## Project Overview

This plugin implements the Model Context Protocol (MCP) over HTTP, allowing AI assistants (Claude Code, Cursor, Windsurf, etc.) to inspect and analyze Unreal Engine projects through a standardized JSON-RPC API.

## GitHub Repositories

| Remote | Repository URL | Purpose |
|--------|----------------|---------|
| `origin` | https://github.com/softdaddy-o/yes-ue-mcp-private.git | Development (private) |
| `public` | https://github.com/softdaddy-o/yes-ue-mcp.git | Release (public) |

## Testing Environment

**Primary Test Project:** Elpis (Action RPG - UE 5.7)
- **Project Path:** `F:\src3\Covenant\ElpisClient\`
- **Plugin Install Path:** `F:\src3\Covenant\ElpisClient\Plugins\yes-ue-mcp\`
- **Version Control:** Perforce (plugin excluded via `.p4ignore`)
- **MCP Endpoint:** `http://127.0.0.1:8080/message`
- **Status:** ✅ Primary test environment - all tools tested here

**Secondary Test Project:** GameAnimationSample57 (UE 5.7)
- **Project Path:** `F:\src_ue5\GameAnimationSample57\`
- **Status:** ❌ Not ready for testing

## Git Workflow

### Private Repository (origin)
- **Commit frequently** - whenever one issue/task is finished
- Use descriptive commit messages
- Push to `origin` for development work

```bash
git add -A && git commit -m "feat: add blueprint analysis tool"
git push origin main
```

### Public Repository (public)
- **Commit aggregated changes** - only when a new version is finished
- Use version tags (v1.0.0, v1.1.0, etc.)
- Push to `public` for releases

```bash
git push public main
git tag v1.0.0
git push public --tags
```

## Module Structure

- **YesUeMcp** (Runtime) - Core MCP protocol layer
- **YesUeMcpEditor** (Editor) - HTTP server + tool implementations

## Coding Standards

- Follow Epic's C++ coding conventions
- Use `YESUEMCP_API` / `YESUEMCPEDITOR_API` for exported symbols
- All tools inherit from `UMcpToolBase`
- Register tools in `FYesUeMcpEditorModule::RegisterBuiltInTools()`

## Key Files

- `Source/YesUeMcp/Public/Tools/McpToolBase.h` - Base class for all tools
- `Source/YesUeMcp/Public/Tools/McpToolRegistry.h` - Tool registration
- `Source/YesUeMcpEditor/Public/Server/McpServer.h` - HTTP server
- `Source/YesUeMcpEditor/Public/Subsystem/McpEditorSubsystem.h` - Lifecycle management

## Project History

### v0.1.0 - Initial Implementation (Dec 2024)

**Commit 4950dce:** Initial commit
- Project structure and build system

**Commit 2c9a428:** feat: initial implementation of yes-ue-mcp plugin
- MCP protocol server using Unreal's HTTP server module
- Tool registry system with lazy instantiation
- Initial 5 tools implemented:
  - `search-assets` - Search by name/class/path with wildcards
  - `analyze-blueprint` - Complete blueprint analysis
  - `get-blueprint-functions` - Function signatures and metadata
  - `get-blueprint-variables` - Variable details with replication info
  - `get-blueprint-components` - Component hierarchy with transforms

**Commit 6173f5a:** fix: UE 5.7 compilation errors
- Fixed deprecated `FBlueprintMetadata::MD_RepNotifyFunc` API
- Updated HTTP Headers from `TMap<FString, FString>` to `TMap<FString, TArray<FString>>`
- Fixed camera animation include paths
- Resolved TUniquePtr forward declaration issues
- Fixed `Response.Error` → `Response.ErrorData` field name

**Commit 4397724:** fix: change MCP endpoint from /mcp to /message
- Changed route from `/mcp` to `/message` for Claude Code compatibility
- Added comprehensive request/response logging
- Implemented GET health check endpoint (was returning 501 error)
- Fixed CORS headers with proper array syntax

### Testing Results (Elpis Project)

✅ **Server Startup**
- MCP server starts automatically with Unreal Editor
- Binds to `http://127.0.0.1:8080/message`
- Health check accessible via GET request

✅ **Claude Code Integration**
- Successfully added with: `claude mcp add --transport http unreal-elpis http://127.0.0.1:8080/message`
- Connection verified with `claude mcp list`
- All tools discoverable via `tools/list`

✅ **Tool Functionality**
- Searched 13 CBP character blueprints successfully
- Analyzed CBP_PC_Base component hierarchy (24 components)
- Retrieved function/variable details from blueprints
- Wildcard pattern matching working correctly

## Current Features

### MCP Server
- **Protocol:** MCP 2024-11-05 with JSON-RPC 2.0
- **Transport:** HTTP (Streamable HTTP ready)
- **Endpoint:** `/message`
- **Port:** 8080 (configurable)
- **CORS:** Enabled for cross-origin requests

### Available Tools (5 total)

| Tool | Description | Status |
|------|-------------|--------|
| `search-assets` | Search assets by pattern, class, or path | ✅ Tested |
| `analyze-blueprint` | Complete blueprint structure analysis | ✅ Tested |
| `get-blueprint-functions` | Function signatures and metadata | ✅ Tested |
| `get-blueprint-variables` | Variable types and properties | ✅ Tested |
| `get-blueprint-components` | Component hierarchy with transforms | ✅ Tested |

## Future Work

### Planned Tools (Priority Order)

#### Phase 1: Level/World Tools (High Priority)
1. **`query-level`** - List actors in currently open level
   - Filter by class, folder, tags
   - Actor names, classes, transforms, components
   - Essential for level understanding

2. **`get-actor-details`** - Inspect specific actor
   - Deep property inspection via reflection
   - Component details with properties
   - Actor references

#### Phase 2: Project Configuration (High Priority)
3. **`get-project-settings`** - Query project configuration
   - Input mappings (actions/axes)
   - Collision profiles and channels
   - Gameplay tags hierarchy
   - Default maps and game modes

#### Phase 3: Analysis Tools (Medium Priority)
4. **`get-class-hierarchy`** - Browse class inheritance
   - Parent chain to UObject
   - Subclasses (C++ and Blueprint)
   - Interfaces and reflection data

5. **`inspect-data-asset`** - DataTable/DataAsset reader
   - Row structure for DataTables
   - Property values for DataAssets
   - Support CurveTable, CompositeDataTable

### Advanced Features (Future Consideration)
- Animation asset query (AnimBP, montages, notifies)
- Material/shader graph inspection
- AI behavior tree analysis
- Write operations (spawn actors, modify properties)
- Streaming support for large datasets
- MCP Resources (expose project files)
- MCP Prompts (template code generation)

## Known Limitations

1. **Read-only** - No modification operations supported
2. **Editor-only** - Cannot run in packaged game
3. **Single session** - No multi-client support
4. **No streaming** - Synchronous responses only
5. **Blueprint focus** - No C++ class introspection yet

## Development Notes

- Plugin source is separate from test project deployment
- Changes tested in Elpis project before committing
- Git commits use conventional commit format (feat/fix/docs/refactor)
- Private repo for development, public repo for releases
