# Logic Graph

Nodes: 4 | Exec Links: 3 | Data Links: 0 | Entry Points: 1 | Orphans: 0

## Graph: Graph
Nodes: 4 | Exec Links: 3 | Data Links: 0 | Entry Points: 1 | Orphans: 0

### Entry Points
- ReceiveTick

### Execution
- ReceiveTick.then -> Branch.execute (kind=exec, confidence=explicit)
- Branch.true -> Print True.execute (kind=exec, confidence=explicit)
- Branch.false -> Print False.execute (kind=exec, confidence=explicit)

### Data Dependencies
- None

### Orphans
- None
