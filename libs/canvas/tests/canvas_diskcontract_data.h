// SPDX-License-Identifier: GPL-3.0-or-later
//
// Byte fixtures for tst_canvas_diskcontract. They live here rather than in the
// test source because moc mis-lexes `#` and `//` inside raw string literals —
// "#Heading" and "https://example.com" both make it give up on the class — and
// moc only scans the .cpp plus headers sharing its base name.
//
// Every expected byte string was produced by transcribing Obsidian 1.13.7's
// `$d`/`Zd` (app/own/0563-$d.js, app/own/0564-Zd.js) into JavaScript and
// running the matching input through it. The literals contain real tab
// characters; keep them that way.
#pragma once

#include <QByteArray>

inline QByteArray canvasP0Fixture()
{
    return QByteArrayLiteral(R"({
            "nodes": [
                {"id":"tbl","type":"table","x":10,"y":20,"width":300,"height":200,"rows":3,"collapsed":false},
                {"id":"txt","type":"text","x":400,"y":20,"width":250,"height":60,"text":"hi"},
                {"id":"grp","x":0,"y":300,"width":600,"height":200,"type":"group","label":"G","backgroundStyle":"cover"}
            ],
            "edges": [
                {"id":"e-kept","fromNode":"tbl","fromSide":"right","toNode":"txt","toSide":"left","label":"wire"},
                {"id":"e-dangle","fromNode":"txt","fromSide":"right","toNode":"ghost","toSide":"left","label":"orphan"}
            ],
            "metadata": {"version":"1.2"}
        })");
}

// Root order nodes, edges, then metadata; modelled nodes and edges in
// getData() order; the two payloads Corbomite does not model ("tbl",
// "e-dangle") with Qt's parse-time key order.
inline QByteArray canvasP0GoldenBytes()
{
    return QByteArrayLiteral(R"({
	"nodes":[
		{"collapsed":false,"height":200,"id":"tbl","rows":3,"type":"table","width":300,"x":10,"y":20},
		{"id":"txt","x":400,"y":20,"width":250,"height":60,"type":"text","text":"hi"},
		{"id":"grp","x":0,"y":300,"width":600,"height":200,"type":"group","label":"G","backgroundStyle":"cover"}
	],
	"edges":[
		{"id":"e-kept","fromNode":"tbl","fromSide":"right","toNode":"txt","toSide":"left","label":"wire"},
		{"fromNode":"txt","fromSide":"right","id":"e-dangle","label":"orphan","toNode":"ghost","toSide":"left"}
	],
	"metadata":{"version":"1.2"}
})");
}

inline QByteArray canvasModelledFixture()
{
    return QByteArrayLiteral(R"({
            "nodes": [
                {"id":"n1","x":0,"y":0,"width":250,"height":60,"color":"1","type":"text","text":"Hello **world**"},
                {"id":"n2","x":300,"y":0,"width":200,"height":80,"type":"text","text":""},
                {"id":"g1","x":-50,"y":-50,"width":600,"height":200,"type":"group","label":"My Group",
                 "background":"img/a.png","backgroundStyle":"repeat"},
                {"id":"f1","x":0,"y":400,"width":400,"height":400,"type":"file","file":"Note.md","subpath":"#Heading"},
                {"id":"l1","x":0,"y":900,"width":400,"height":400,"type":"link","url":"https://example.com"},
                {"id":"x1","x":0,"y":0,"width":10,"height":10,"type":"text","text":"one extra field",
                 "futureField":42,"meta":{"a":1}}
            ],
            "edges": [
                {"id":"e1","fromNode":"n1","fromSide":"right","toNode":"n2","toSide":"left",
                 "fromEnd":"none","toEnd":"arrow","color":"","label":"relates to"}
            ]
        })");
}

// `text:""`, `fromEnd:"none"`, `toEnd:"arrow"` and `color":""` all survive,
// per the F5 rule in the addendum; `x1`'s unmodelled fields lead, sorted.
inline QByteArray canvasModelledGoldenBytes()
{
    return QByteArrayLiteral(R"({
	"nodes":[
		{"id":"n1","x":0,"y":0,"width":250,"height":60,"color":"1","type":"text","text":"Hello **world**"},
		{"id":"n2","x":300,"y":0,"width":200,"height":80,"type":"text","text":""},
		{"id":"g1","x":-50,"y":-50,"width":600,"height":200,"type":"group","label":"My Group","background":"img/a.png","backgroundStyle":"repeat"},
		{"id":"f1","x":0,"y":400,"width":400,"height":400,"type":"file","file":"Note.md","subpath":"#Heading"},
		{"id":"l1","x":0,"y":900,"width":400,"height":400,"type":"link","url":"https://example.com"},
		{
			"futureField":42,
			"meta":{"a":1},
			"id":"x1",
			"x":0,
			"y":0,
			"width":10,
			"height":10,
			"type":"text",
			"text":"one extra field"
		}
	],
	"edges":[
		{"id":"e1","fromNode":"n1","fromSide":"right","toNode":"n2","toSide":"left","fromEnd":"none","toEnd":"arrow","color":"","label":"relates to"}
	]
})");
}
