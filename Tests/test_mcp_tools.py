#!/usr/bin/env python3
"""
MCP Tools Integration Tests

This script tests the yes-ue-mcp plugin tools using Python's unittest framework.

Usage:
    python -m pytest test_mcp_tools.py -v
    python -m unittest test_mcp_tools -v
    python test_mcp_tools.py

Requirements:
    - Unreal Editor running with yes-ue-mcp plugin
    - MCP server started (check toolbar icon)
    - pip install requests pytest (optional for pytest runner)

Environment Variables:
    MCP_HOST: MCP server host (default: 127.0.0.1)
    MCP_PORT: MCP server port (default: 8080)
"""

import json
import os
import unittest
import requests
from typing import Any, Dict, List, Optional


class McpClient:
    """Client for making MCP JSON-RPC requests."""

    def __init__(self, host: str = "127.0.0.1", port: int = 8080):
        self.url = f"http://{host}:{port}/message"
        self.request_id = 0

    def call_tool(self, tool_name: str, arguments: Dict[str, Any] = None) -> Dict[str, Any]:
        """Call an MCP tool and return the result."""
        self.request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": str(self.request_id),
            "method": "tools/call",
            "params": {
                "name": tool_name,
                "arguments": arguments or {}
            }
        }

        response = requests.post(
            self.url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=30
        )
        response.raise_for_status()

        result = response.json()
        if "error" in result:
            raise McpError(result["error"].get("message", "Unknown error"))

        # Parse the content from the result
        content = result.get("result", {}).get("content", [])
        if content and content[0].get("type") == "text":
            text = content[0]["text"]
            if text.strip():
                return json.loads(text)
        return result.get("result", {})

    def list_tools(self) -> List[Dict[str, Any]]:
        """List all available tools."""
        self.request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": str(self.request_id),
            "method": "tools/list"
        }

        response = requests.post(
            self.url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=10
        )
        response.raise_for_status()

        result = response.json()
        return result.get("result", {}).get("tools", [])


class McpError(Exception):
    """Exception raised for MCP errors."""
    pass


class McpTestCase(unittest.TestCase):
    """Base test case with MCP client setup."""

    client: McpClient = None

    @classmethod
    def setUpClass(cls):
        """Set up MCP client for all tests."""
        host = os.environ.get("MCP_HOST", "127.0.0.1")
        port = int(os.environ.get("MCP_PORT", "8080"))
        cls.client = McpClient(host, port)

        # Verify connection
        try:
            tools = cls.client.list_tools()
            if not tools:
                raise unittest.SkipTest("No tools available from MCP server")
        except requests.exceptions.ConnectionError:
            raise unittest.SkipTest(
                f"Could not connect to MCP server at {host}:{port}. "
                "Make sure Unreal Editor is running with the plugin."
            )


class TestConnection(McpTestCase):
    """Test MCP server connection."""

    def test_server_responds(self):
        """Test that the MCP server responds to requests."""
        tools = self.client.list_tools()
        self.assertIsInstance(tools, list)
        self.assertGreater(len(tools), 0)

    def test_minimum_tools_available(self):
        """Test that expected minimum number of tools are available."""
        tools = self.client.list_tools()
        tool_names = [t["name"] for t in tools]
        # Should have at least 35 tools
        self.assertGreaterEqual(len(tool_names), 35,
            f"Expected at least 35 tools, got {len(tool_names)}")

    def test_required_read_tools_exist(self):
        """Test that essential read tools are registered."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        required_tools = [
            "get-project-info",
            "query-level",
            "search-assets",
            "analyze-blueprint",
            "get-blueprint-graph",
        ]
        for tool in required_tools:
            self.assertIn(tool, tool_names, f"Required tool '{tool}' not found")

    def test_required_write_tools_exist(self):
        """Test that essential write tools are registered."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        required_tools = [
            "spawn-actor",
            "delete-actor",
            "create-asset",
            "delete-asset",
            "compile-blueprint",
        ]
        for tool in required_tools:
            self.assertIn(tool, tool_names, f"Required tool '{tool}' not found")


class TestReadTools(McpTestCase):
    """Test read-only MCP tools."""

    def test_get_project_info(self):
        """Test get-project-info returns valid project data."""
        result = self.client.call_tool("get-project-info")

        self.assertIn("project_name", result)
        self.assertIn("project_path", result)
        self.assertIn("plugin", result)
        self.assertIn("version", result["plugin"])

    def test_query_level(self):
        """Test query-level returns actor list."""
        result = self.client.call_tool("query-level", {"limit": 5})

        self.assertIn("actors", result)
        self.assertIn("level_name", result)
        self.assertIsInstance(result["actors"], list)

    def test_query_level_with_filter(self):
        """Test query-level with class filter."""
        result = self.client.call_tool("query-level", {
            "class_filter": "PlayerStart",
            "limit": 10
        })

        self.assertIn("actors", result)
        # All returned actors should match filter
        for actor in result["actors"]:
            self.assertIn("PlayerStart", actor["class"])

    def test_search_assets(self):
        """Test search-assets returns results."""
        result = self.client.call_tool("search-assets", {
            "query": "*",
            "limit": 5
        })

        self.assertIn("assets", result)
        self.assertIn("count", result)

    def test_get_logs(self):
        """Test get-logs returns log entries."""
        result = self.client.call_tool("get-logs", {"limit": 10})

        self.assertIn("entries", result)
        self.assertIsInstance(result["entries"], list)


class TestActorLifecycle(McpTestCase):
    """Test actor spawn and delete operations."""

    def setUp(self):
        """Track actors to clean up."""
        self.spawned_actors = []

    def tearDown(self):
        """Clean up any spawned actors."""
        for actor_name in self.spawned_actors:
            try:
                self.client.call_tool("delete-actor", {"actor_name": actor_name})
            except:
                pass

    def test_spawn_actor(self):
        """Test spawning an actor in the level."""
        result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "location": [100, 200, 300],
            "label": "MCP_Test_SpawnActor"
        })

        self.assertTrue(result.get("success"), "spawn-actor should succeed")
        self.assertIn("actor_name", result)
        self.spawned_actors.append(result["actor_name"])

    def test_spawn_and_verify_actor(self):
        """Test that spawned actor appears in level query."""
        label = "MCP_Test_VerifySpawn"

        # Spawn
        spawn_result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "location": [0, 0, 500],
            "label": label
        })
        self.assertTrue(spawn_result.get("success"))
        self.spawned_actors.append(spawn_result["actor_name"])

        # Verify
        query_result = self.client.call_tool("query-level", {
            "class_filter": "*Light*"
        })
        actor_labels = [a.get("label") for a in query_result.get("actors", [])]
        self.assertIn(label, actor_labels,
            f"Spawned actor '{label}' should appear in level query")

    def test_delete_actor(self):
        """Test deleting an actor from the level."""
        # First spawn an actor
        spawn_result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "label": "MCP_Test_DeleteActor"
        })
        self.assertTrue(spawn_result.get("success"))
        actor_name = spawn_result["actor_name"]

        # Delete it
        delete_result = self.client.call_tool("delete-actor", {
            "actor_name": actor_name
        })
        self.assertTrue(delete_result.get("success"), "delete-actor should succeed")

        # Verify it's gone
        query_result = self.client.call_tool("query-level", {
            "class_filter": "*Light*"
        })
        actor_names = [a.get("name") for a in query_result.get("actors", [])]
        self.assertNotIn(actor_name, actor_names,
            f"Deleted actor '{actor_name}' should not appear in level query")


class TestAssetLifecycle(McpTestCase):
    """Test asset create and delete operations."""

    def setUp(self):
        """Track assets to clean up."""
        self.created_assets = []

    def tearDown(self):
        """Clean up any created assets."""
        for asset_path in self.created_assets:
            try:
                self.client.call_tool("delete-asset", {"asset_path": asset_path})
            except:
                pass

    def test_create_blueprint(self):
        """Test creating a Blueprint asset."""
        asset_path = "/Game/BP_MCP_TestCreate"

        result = self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })

        self.assertTrue(result.get("success"), "create-asset should succeed")
        self.created_assets.append(asset_path)

    def test_create_and_verify_blueprint(self):
        """Test that created Blueprint can be analyzed."""
        asset_path = "/Game/BP_MCP_TestVerify"

        # Create
        create_result = self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        self.assertTrue(create_result.get("success"))
        self.created_assets.append(asset_path)

        # Verify with analyze
        analyze_result = self.client.call_tool("analyze-blueprint", {
            "asset_path": asset_path,
            "include_graph": False
        })
        self.assertEqual(analyze_result.get("parent_class"), "Actor")

    def test_delete_asset(self):
        """Test deleting an asset."""
        asset_path = "/Game/BP_MCP_TestDelete"

        # Create first
        self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint"
        })

        # Delete
        delete_result = self.client.call_tool("delete-asset", {
            "asset_path": asset_path
        })
        self.assertTrue(delete_result.get("success"), "delete-asset should succeed")

        # Verify it's gone
        search_result = self.client.call_tool("search-assets", {
            "query": "BP_MCP_TestDelete"
        })
        self.assertEqual(search_result.get("count", 0), 0,
            "Deleted asset should not appear in search")


class TestBlueprintModification(McpTestCase):
    """Test Blueprint graph modification tools."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint for modification tests."""
        super().setUpClass()
        cls.test_bp_path = "/Game/BP_MCP_ModificationTest"

        # Create test Blueprint
        result = cls.client.call_tool("create-asset", {
            "asset_path": cls.test_bp_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        if not result.get("success"):
            raise unittest.SkipTest("Could not create test Blueprint")

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {
                "asset_path": cls.test_bp_path
            })
        except:
            pass
        super().tearDownClass()

    def test_add_graph_node(self):
        """Test adding a node to Blueprint graph."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "VariableGet",
            "variable_name": "DefaultSceneRoot",
            "graph_name": "EventGraph"
        })

        self.assertTrue(result.get("success"), "add-graph-node should succeed")
        self.assertTrue(result.get("needs_compile"), "Should need compile after adding node")

    def test_compile_blueprint(self):
        """Test compiling a Blueprint."""
        result = self.client.call_tool("compile-blueprint", {
            "asset_path": self.test_bp_path
        })

        self.assertTrue(result.get("success"), "compile-blueprint should succeed")
        self.assertIn(result.get("status"), ["compiled", "compiled_with_warnings"])

    def test_graph_modification_reflected(self):
        """Test that graph modifications are visible in get-blueprint-graph."""
        # Add a node
        add_result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "VariableGet",
            "variable_name": "DefaultSceneRoot",
            "graph_name": "EventGraph"
        })
        self.assertTrue(add_result.get("success"))

        # Read graph
        graph_result = self.client.call_tool("get-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "graph_type": "event"
        })

        # Find VariableGet node
        found = False
        for graph in graph_result.get("graphs", []):
            for node in graph.get("nodes", []):
                if "VariableGet" in node.get("class", ""):
                    found = True
                    break

        self.assertTrue(found, "Added VariableGet node should appear in graph")


if __name__ == "__main__":
    unittest.main(verbosity=2)
