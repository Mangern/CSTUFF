# LSP for LANG

Tired of compiler development, let's make LSP.

## Base protocol.
- Synthesize JSON content
- Calculate header: Content-Length
- utf-8?


## Types
- integer
- uinteger
- decimal
- LSPAny
- LSPObject
- LSPArray
- Message types

## JSON parser
hehe...

## Lifecycle
- Implement (event?) loop
- Implement 'intitialize' request
    - Read capabilities (not important for now)
    - Read rootUri / path
    - Respond with own capabilities
- Then basically handle didUpdate and all the fun stuff
