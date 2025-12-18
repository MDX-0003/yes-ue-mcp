# Testing Guide for yes-ue-mcp

This guide provides testing instructions for all implemented MCP tools.

## Prerequisites

1. **Unreal Engine 5.7+** with the yes-ue-mcp plugin installed
2. **Test Project**: GameAnimationSample57 (or your own UE project)
   - Project Path: `F:\src_ue5\GameAnimationSample57\`
3. **MCP Client**: Claude Code, Claude Desktop, Cursor, or another MCP-compatible client
4. **Editor Running**: Start Unreal Editor with your test project

## Server Health Check

Before testing tools, verify the server is running:

```bash
curl http://127.0.0.1:8080/message
```

Expected response:
```json
{
  "name": "yes-ue-mcp",
  "version": "1.0.0",
  "protocol": "2024-11-05",
  "running": true
}
```

## Tool Testing

### 1. Blueprint Analysis Tools

#### Test 1.1: `analyze-blueprint`

**Objective**: Get complete Blueprint structure

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/call",
    "params": {
      "name": "analyze-blueprint",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns JSON with Blueprint metadata
- ✅ Includes parent class name
- ✅ Lists all functions
- ✅ Lists all variables
- ✅ Lists all components

---

#### Test 1.2: `get-blueprint-functions`

**Objective**: List all Blueprint functions

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-functions",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns array of functions
- ✅ Each function has name, inputs, outputs
- ✅ Function flags are included
- ✅ Filtering works (test with `function_filter`)

---

#### Test 1.3: `get-blueprint-variables`

**Objective**: List all Blueprint variables

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-variables",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns array of variables
- ✅ Each variable has name, type, default value
- ✅ Replication info is included
- ✅ Category information is present

---

#### Test 1.4: `get-blueprint-components`

**Objective**: Get component hierarchy

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 4,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-components",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base",
        "include_transforms": true
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns component hierarchy
- ✅ Parent-child relationships are correct
- ✅ Transforms included (location, rotation, scale)
- ✅ Attachment info is present

---

#### Test 1.5: `get-blueprint-graph`

**Objective**: Read complete graph structure

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 5,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-graph",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base",
        "graph_type": "event",
        "include_positions": true
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns all event graphs
- ✅ Each graph has nodes array
- ✅ Nodes have GUIDs, classes, titles
- ✅ Pins with connections are listed
- ✅ Node positions included

---

#### Test 1.6: `get-blueprint-node`

**Objective**: Get specific node details

**Prerequisites**: Get a node GUID from `get-blueprint-graph` first

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 6,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-node",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base",
        "node_guid": "<GUID_FROM_GRAPH>"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns node details
- ✅ All pins listed with types
- ✅ Connections show target nodes
- ✅ Node comment included (if any)

---

#### Test 1.7: `get-blueprint-defaults`

**Objective**: Read CDO property defaults

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 7,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-defaults",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base",
        "property_filter": "*Health*"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns property array
- ✅ Each property has name, type, value
- ✅ Property flags included
- ✅ Filtering works correctly

---

### 2. Level/World Tools

#### Test 2.1: `query-level`

**Objective**: List actors in current level

**Prerequisites**: Open a level in the editor

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 8,
    "method": "tools/call",
    "params": {
      "name": "query-level",
      "arguments": {
        "class_filter": "*Light*",
        "include_transform": true,
        "limit": 50
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns actors matching filter
- ✅ Actor names and classes listed
- ✅ Transforms included
- ✅ Limit is respected

---

#### Test 2.2: `get-actor-details`

**Objective**: Deep inspect specific actor

**Prerequisites**: Get actor name from `query-level`

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 9,
    "method": "tools/call",
    "params": {
      "name": "get-actor-details",
      "arguments": {
        "actor_name": "DirectionalLight_0",
        "include_properties": true,
        "include_components": true
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns complete actor details
- ✅ Properties listed with values
- ✅ Components with hierarchy
- ✅ Transform data included

---

### 3. Project Configuration

#### Test 3.1: `get-project-settings`

**Objective**: Query project configuration

**Test Case - All Sections**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 10,
    "method": "tools/call",
    "params": {
      "name": "get-project-settings",
      "arguments": {
        "section": "all"
      }
    }
  }'
```

**Test Case - Input Only**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 11,
    "method": "tools/call",
    "params": {
      "name": "get-project-settings",
      "arguments": {
        "section": "input"
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns requested sections
- ✅ Input: action/axis mappings present
- ✅ Collision: profiles and channels listed
- ✅ Tags: tag sources included
- ✅ Maps: default maps listed

---

### 4. Analysis Tools

#### Test 4.1: `get-class-hierarchy`

**Objective**: Browse class inheritance

**Test Case - Parents**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 12,
    "method": "tools/call",
    "params": {
      "name": "get-class-hierarchy",
      "arguments": {
        "class_name": "ACharacter",
        "direction": "parents"
      }
    }
  }'
```

**Test Case - Children**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 13,
    "method": "tools/call",
    "params": {
      "name": "get-class-hierarchy",
      "arguments": {
        "class_name": "AActor",
        "direction": "children",
        "include_blueprints": true
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns class hierarchy
- ✅ Parents: chain to UObject
- ✅ Children: direct subclasses
- ✅ Blueprint classes included/excluded per flag

---

#### Test 4.2: `inspect-data-asset`

**Objective**: Read DataTable/DataAsset contents

**Test Case - DataTable**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 14,
    "method": "tools/call",
    "params": {
      "name": "inspect-data-asset",
      "arguments": {
        "asset_path": "/Game/Data/DT_Items"
      }
    }
  }'
```

**Expected Results**:
- ✅ Type identified (DataTable or DataAsset)
- ✅ DataTable: all rows listed
- ✅ Row data with field names and values
- ✅ Filtering works (test with `row_filter`)

---

### 5. Asset Management

#### Test 5.1: `search-assets`

**Objective**: Search assets by pattern

**Test Case**:
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 15,
    "method": "tools/call",
    "params": {
      "name": "search-assets",
      "arguments": {
        "pattern": "CBP*",
        "class_filter": "Blueprint",
        "limit": 20
      }
    }
  }'
```

**Expected Results**:
- ✅ Returns matching assets
- ✅ Asset paths correct
- ✅ Class filter works
- ✅ Wildcard matching works

---

## Integration Testing with Claude Code

### Test 1: Discover Tools

```bash
claude mcp invoke unreal-elpis tools/list
```

**Expected**: List of all 13 tools

---

### Test 2: Analyze Blueprint

Ask Claude Code:
```
"Analyze the CBP_PC_Base Blueprint and tell me about its components"
```

**Expected**: Claude uses `analyze-blueprint` and `get-blueprint-components` to describe the Blueprint structure

---

### Test 3: Query Level

Ask Claude Code:
```
"List all lights in the current level"
```

**Expected**: Claude uses `query-level` with `class_filter: "*Light*"` and lists the lights

---

### Test 4: Inspect Actor

Ask Claude Code:
```
"Tell me about the DirectionalLight_0 actor in the level"
```

**Expected**: Claude uses `get-actor-details` to provide detailed actor information

---

## Error Cases to Test

### Invalid Asset Path
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 100,
    "method": "tools/call",
    "params": {
      "name": "analyze-blueprint",
      "arguments": {
        "asset_path": "/Game/DoesNotExist"
      }
    }
  }'
```

**Expected**: Error message indicating asset not found

---

### Missing Required Parameter
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 101,
    "method": "tools/call",
    "params": {
      "name": "analyze-blueprint",
      "arguments": {}
    }
  }'
```

**Expected**: Error message indicating missing required parameter

---

### Invalid Filter Value
```bash
curl -X POST http://127.0.0.1:8080/message \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 102,
    "method": "tools/call",
    "params": {
      "name": "get-blueprint-graph",
      "arguments": {
        "asset_path": "/Game/Elpis2/Characters/PC/PC_Base/CBP_PC_Base",
        "graph_type": "invalid"
      }
    }
  }'
```

**Expected**: Error message indicating invalid graph_type value

---

## Performance Testing

### Large Blueprint Graph
- Test `get-blueprint-graph` on a complex Blueprint with 100+ nodes
- Verify response time is reasonable (<2 seconds)

### Level with Many Actors
- Test `query-level` on a level with 1000+ actors
- Verify limit parameter works correctly
- Check response time with different limits

---

## Test Checklist

- [ ] Server starts successfully
- [ ] Health check endpoint works
- [ ] All 13 tools are discoverable via `tools/list`
- [ ] Blueprint tools work with test Blueprint
- [ ] Level tools work with open level
- [ ] Project settings return valid data
- [ ] Class hierarchy traversal works
- [ ] DataTable inspection works
- [ ] Asset search returns results
- [ ] Error handling works correctly
- [ ] Claude Code integration works
- [ ] Filtering parameters work
- [ ] Optional parameters have correct defaults

---

## Troubleshooting

### Server Not Starting
- Check Output Log in UE Editor for error messages
- Verify port 8080 is not in use by another application
- Check plugin is enabled in Project Settings

### Tool Returns Empty Results
- Verify asset paths are correct (use Content Browser to copy paths)
- Check if level is open for level tools
- Ensure Blueprints are compiled

### Connection Refused
- Confirm UE Editor is running
- Verify server started (check Output Log)
- Test health check endpoint first

---

## Report Issues

If you encounter any issues during testing:

1. Check the UE Editor Output Log for error messages
2. Note the exact tool name and parameters used
3. Include the error response (if any)
4. Report at: https://github.com/softdaddy-o/yes-ue-mcp/issues
