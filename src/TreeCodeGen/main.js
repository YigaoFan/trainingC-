const text = document.querySelector('#id-log')
const printCode = function(s) {
    text.value += s
    text.value += '\n'
}
const printTreeCode = function(root) {
    var leafIdx = 0
    var middleIdx = 0
    root.traverse(function (data, subNodeNames) {
        var name = null
        if (subNodeNames.length == 0) {
            name = `leafNode${leafIdx++}`
        } else {
            name = `middleNode${middleIdx++}`
        }
        printCode(`auto ${name} = LabelNode(${data}, { ${subNodeNames.join(', ')} });`)
        return name
    })
}
const printReadStateNodeCode = function(root) {
    var leafIdx = 0
    var middleIdx = 0
    root.traverseSubIf(n => n.read,
        function(node, subNodeNames) {
            var name = null
            var read = node.read || false
            if (subNodeNames.length == 0) {
                name = `leafNode${leafIdx++}`
            } else {
                name = `middleNode${middleIdx++}`
            }
            printCode(`auto ${name} = ReadStateLabelNode(${node.data}, { ${subNodeNames.join(', ')} },  ${read});`)
            return name
        })

}

const getWindow = function(id) {
    var canvas = document.querySelector(id)
    var context = canvas.getContext('2d')
    return {
        canvas,
        context,
    }
}

const __main = function() {
    var mainWindow = getWindow('#id-canvas')
    var partWindow = getWindow('#id-part-canvas')
    
    var root = new Node(0, 275, 20)
    var freeNode = new Node(0)
    var partControlIds = [
        '#id-part-default-checkbox',
        '#id-part-delete-checkbox',
        '#id-part-move-checkbox',
    ]
    var entireControlIds = [
        '#id-entire-default-checkbox',
        '#id-entire-delete-checkbox',
        '#id-entire-move-checkbox',
        '#id-entire-part-checkbox',
    ]
    // var partEdit = null
    var partEdit = new PartEdit(partWindow, partControlIds)
    var mainEdit = new EntireEdit(mainWindow, root, freeNode, partEdit, entireControlIds)

    window.addEventListener('keydown', function (event) {
        var k = event.key
        mainEdit.onKeyDown(k)
    })
    window.addEventListener('keyup', function (event) {
        var k = event.key
        mainEdit.onKeyUp(k)
    })

    var generateButton = document.querySelector('#id-generate-button')

    var levels = [
        {
            name: 'Store original tree state',
            handler : function (event) {
                printCode("original:")
                printTreeCode(root)
            }
        },
        {
            name: 'Generate tree code after change',
            handler : function (event) {
                printCode("after modify:")
                printTreeCode(root)

                printCode("delete nodes:")
                printTreeCode(freeNode)
            }
        }
    ]

    var currentLevel = 0
    generateButton.innerText = levels[currentLevel].name
    generateButton.addEventListener('click', function(event) {
        levels[currentLevel].handler(event)
        ++currentLevel
        if (!(currentLevel < levels.length)) {
            currentLevel = 0
            printCode('')
            printCode('')
        }

        generateButton.innerText = levels[currentLevel].name
    })
    var clearButton = document.querySelector('#id-clear-button')
    clearButton.addEventListener('click', function (event) {
        text.value = ''
        freeNode.children = []
    })

    setInterval(function() {
        mainEdit.draw()
        // partEdit.draw()
    }, 1000/30)
}

__main()