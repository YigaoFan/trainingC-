const distance = function(x1, y1, x2, y2) {
    return Math.sqrt(Math.pow(x1 - x2, 2) + Math.pow(y1 - y2, 2))
}
/// point is { x, y }
/// line is [point1, point2]
const distanceToLine = function(point, line) {
    var p1 = point
    var p2 = line[0]
    var p3 = line[1]
    var A = distance(p1.x, p1.y, p2.x, p2.y)
    var B = distance(p1.x, p1.y, p3.x, p3.y)
    var C = distance(p2.x, p2.y, p3.x, p3.y)
    // 利用海伦公式计算三角形面积
    var P = (A + B + C) / 2
    var area = Math.sqrt(P * (P - A) * (P - B) * (P - C))

    var dis = (2 * area) / C
    return dis
}

const Node = function(data, x = null, y = null, parent = null) {
    var o = {
        x,
        y,
        width: 50,
        height: 50,
        data,
        children: [],
        parent,
        read: null,
        deleted: null,
    }

    o.bornChild = function() {
        var c = Node(getData(), o.x, o.y, o)
        o.children.push(c)
        o.children.sort((a, b) => a.data - b.data)
        return c
    }

    o.locateNode = function(x, y) {
        if (x >= o.x && x <= o.x + o.width) {
            if (y >= o.y && y <= o.y + o.height) {
                return o
            }
        }

        for (var i = 0; i < o.children.length; ++i) {
            var c = o.children[i]
            var n = c.locateNode(x, y)
            if (n != null) {
                return n
            }
        }

        return null
    }

    o.locateRelation = function(x, y) {
        var distanceLimit = 5
        var p1 = o.getMiddlePoint()
        for (const c of o.children) {
            var p2 = c.getMiddlePoint()
            if (distanceToLine({ x, y, }, [p1, p2, ]) < distanceLimit) {
                return c
            }

            var n = c.locateRelation(x, y)
            if (n != null) {
                return n
            }
        }

        return null
    }

    o.draw = function(context) {
        context.fillStyle = 'black'
        context.strokeRect(o.x, o.y, o.width, o.height)
        context.fillStyle = 'green'
        context.fillRect(o.x, o.y, o.width, o.height)
        context.beginPath()
        context.fillStyle = 'black'
        context.font = '20px Arial'
        context.textAlign = "center"
        var p1 = o.getMiddlePoint()

        var s = data.toString()
        if (o.deleted == true) {
            s += ' d'
        }
        if (o.read == true) {
            s += ' r'
        }
        context.fillText(s, p1.x, p1.y)

        o.children.forEach(c => {
            c.draw(context)
            context.moveTo(p1.x, p1.y)
            context.fillStyle = 'black'
            var p2 = c.getMiddlePoint()
            context.lineTo(p2.x, p2.y)
            context.stroke()
        })
    }

    o.getMiddlePoint = function() {
        return {
            x: (o.x + o.x + o.width) / 2,
            y: (o.y + o.y + o.height) / 2,
        }
    }

    o.traverse = function(callback) {
        var subResults = []
        o.children.forEach(e => {
            var r = e.traverse(callback)
            subResults.push(r)
        })

        return callback(o.data, subResults)
    }

    o.remove = function(child) {
        o.children = o.children.filter(e => {
            return e != child
        })
    }

    o.addChild = function(node) {
        node.parent = o
        o.children.push(node)
        o.children.sort((a, b) => a.data - b.data)
    }

    o.insertParent = function(node) {
        o.parent.remove(o)
        o.parent.addChild(node)
        node.addChild(o)
    }

    o.giveChildren = function() {
        var c = o.children
        o.children = []
        return c
    }

    o.searchClosestNode = function(x, y) {
        var p = o.getMiddlePoint()
        var minDistanceNode = o
        var minDistance = distance(x, y, p.x, p.y)

        for (let c of o.children) {
            var n = c.searchClosestNode(x, y)
            var d = distance(n.x, n.y, x, y)
            if (d < minDistance) {
                minDistance = d
                minDistanceNode = n
            }
        }

        return minDistanceNode
    }

    o.clone = function() {
        var children = []
        o.children.forEach(x => children.push(x.clone()))

        var n = Node(o.data, o.x, o.y)
        n.children = children
        return  n
    }

    return o
}