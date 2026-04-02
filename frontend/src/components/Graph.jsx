import { useMemo, useState } from "react";

export default function Graph({ graph, contradictions }) {
  const [selectedNode, setSelectedNode] = useState("");

  const layout = useMemo(() => {
    const nodes = graph.nodes || [];
    const radius = 230;
    const centerX = 360;
    const centerY = 280;

    return nodes.map((node, index) => {
      const angle = (Math.PI * 2 * index) / Math.max(nodes.length, 1);
      return {
        ...node,
        x: centerX + Math.cos(angle) * radius,
        y: centerY + Math.sin(angle) * radius,
      };
    });
  }, [graph]);

  const nodesById = useMemo(
    () => new Map(layout.map((node) => [node.id, node])),
    [layout],
  );

  return (
    <section className="panel-grid graph-layout">
      <div className="panel graph-panel">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">Knowledge graph</p>
            <h3>Relationships between notes and concepts</h3>
          </div>
          <div className="card-badge">{layout.length} nodes</div>
        </div>

        <div className="graph-stage">
          <svg
            viewBox="0 0 720 560"
            className="graph-svg"
            aria-label="Knowledge graph"
          >
            {(graph.edges || []).map((edge, index) => {
              const source = nodesById.get(edge.source);
              const target = nodesById.get(edge.target);
              if (!source || !target) {
                return null;
              }
              const highlighted =
                selectedNode &&
                (selectedNode === edge.source || selectedNode === edge.target);
              return (
                <line
                  key={`${edge.source}-${edge.target}-${index}`}
                  x1={source.x}
                  y1={source.y}
                  x2={target.x}
                  y2={target.y}
                  className={
                    highlighted ? "graph-edge highlighted" : "graph-edge"
                  }
                />
              );
            })}

            {layout.map((node) => (
              <g
                key={node.id}
                onClick={() => setSelectedNode(node.id)}
                className="graph-node-group"
              >
                <circle
                  cx={node.x}
                  cy={node.y}
                  r={node.type === "note" ? 26 : 18}
                  className={
                    selectedNode === node.id
                      ? "graph-node active"
                      : "graph-node"
                  }
                />
                <text
                  x={node.x}
                  y={node.y + 44}
                  textAnchor="middle"
                  className="graph-label"
                >
                  {node.label}
                </text>
              </g>
            ))}
          </svg>
        </div>
      </div>

      <aside className="side-column">
        <div className="panel compact-panel">
          <p className="panel-kicker">Contradictions</p>
          <ul className="compact-list">
            {(contradictions || []).slice(0, 5).map((item, index) => (
              <li key={`${item.left}-${item.right}-${index}`}>
                <strong>
                  {item.left} vs {item.right}
                </strong>
                <span>{item.summary}</span>
              </li>
            ))}
          </ul>
        </div>

        <div className="panel compact-panel">
          <p className="panel-kicker">Selected node</p>
          <p>
            {selectedNode
              ? nodesById.get(selectedNode)?.label
              : "Click a node to inspect it."}
          </p>
        </div>
      </aside>
    </section>
  );
}
