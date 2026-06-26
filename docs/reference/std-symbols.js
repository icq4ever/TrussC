// Curated std::-provided symbols that are AST-invisible (pure std::, not
// re-exported into trussc::) but available to user code via `using namespace std`.
// structure.js merges these with provider:"std" so they appear in the reference
// (people expect sin/cos/lerp). Prose lives in api-reference.toml, keyed by id.
module.exports = [
  {
    "id": "lerp",
    "kind": "func",
    "name": "lerp",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float a, float b, float t",
        "const": false
      }
    ]
  },
  {
    "id": "sin",
    "kind": "func",
    "name": "sin",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "cos",
    "kind": "func",
    "name": "cos",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "tan",
    "kind": "func",
    "name": "tan",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "asin",
    "kind": "func",
    "name": "asin",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "acos",
    "kind": "func",
    "name": "acos",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "atan",
    "kind": "func",
    "name": "atan",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "atan2",
    "kind": "func",
    "name": "atan2",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float y, float x",
        "const": false
      }
    ]
  },
  {
    "id": "abs",
    "kind": "func",
    "name": "abs",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "sqrt",
    "kind": "func",
    "name": "sqrt",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "pow",
    "kind": "func",
    "name": "pow",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x, float y",
        "const": false
      }
    ]
  },
  {
    "id": "log",
    "kind": "func",
    "name": "log",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "exp",
    "kind": "func",
    "name": "exp",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "min",
    "kind": "func",
    "name": "min",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float a, float b",
        "const": false
      }
    ]
  },
  {
    "id": "max",
    "kind": "func",
    "name": "max",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float a, float b",
        "const": false
      }
    ]
  },
  {
    "id": "floor",
    "kind": "func",
    "name": "floor",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "ceil",
    "kind": "func",
    "name": "ceil",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "round",
    "kind": "func",
    "name": "round",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x",
        "const": false
      }
    ]
  },
  {
    "id": "fmod",
    "kind": "func",
    "name": "fmod",
    "provider": "std",
    "signatures": [
      {
        "ret": "float",
        "params": "float x, float y",
        "const": false
      }
    ]
  }
];
