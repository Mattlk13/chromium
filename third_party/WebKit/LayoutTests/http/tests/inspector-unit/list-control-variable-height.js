TestRunner.addResult('Test ListControl rendering for variable height case.');

class Delegate {
  constructor() {
  }

  createElementForItem(item) {
    TestRunner.addResult('Creating element for ' + item);
    var element = document.createElement('div');
    element.style.height = this.heightForItem(item) + 'px';
    element.textContent = item;
    return element;
  }

  heightForItem(item) {
    return 7 + item % 10;
  }

  isItemSelectable(item) {
    return (item % 5 === 0) || (item % 5 === 2);
  }

  selectedItemChanged(from, to, fromElement, toElement) {
    TestRunner.addResult('Selection changed from ' + from + ' to ' + to);
    if (fromElement)
      fromElement.classList.remove('selected');
    if (toElement)
      toElement.classList.add('selected');
  }
}

var delegate = new Delegate();
var list = new UI.ListControl(delegate, UI.ListMode.ViewportVariableItems);
list.element.style.height = '73px';
UI.inspectorView.element.appendChild(list.element);

function dumpList()
{
  var height = list.element.offsetHeight;
  TestRunner.addResult(`----list[length=${list.length()}][height=${height}]----`);
  for (var child of list.element.children) {
    var offsetTop = child.getBoundingClientRect().top - list.element.getBoundingClientRect().top;
    var offsetBottom = child.getBoundingClientRect().bottom - list.element.getBoundingClientRect().top;
    var visible = (offsetBottom <= 0 || offsetTop >= height) ? ' ' :
        (offsetTop >= 0 && offsetBottom <= height ? '*' : '+');
    var selected = child.classList.contains('selected') ? ' (selected)' : '';
    var text = child === list._topElement ? 'top' : (child === list._bottomElement ? 'bottom' : child.textContent);
    TestRunner.addResult(`${visible}[${offsetTop}] ${text}${selected}`);
  }
  TestRunner.addResult('offsets: ' + list._variableOffsets.join(' '));
  TestRunner.addResult('');
}

TestRunner.addResult('Adding 0, 1, 2');
list.replaceAllItems([0, 1, 2]);
dumpList();

TestRunner.addResult('Scrolling to 0');
list.scrollItemAtIndexIntoView(0);
dumpList();

TestRunner.addResult('Scrolling to 2');
list.scrollItemAtIndexIntoView(2);
dumpList();

TestRunner.addResult('Adding 3-20');
list.replaceItemsInRange(3, 3, [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
dumpList();

TestRunner.addResult('Scrolling to 11');
list.scrollItemAtIndexIntoView(11);
dumpList();

TestRunner.addResult('Scrolling to 19');
list.scrollItemAtIndexIntoView(19);
dumpList();

TestRunner.addResult('Scrolling to 16');
list.scrollItemAtIndexIntoView(16);
dumpList();

TestRunner.addResult('Scrolling to 3');
list.scrollItemAtIndexIntoView(3);
dumpList();

TestRunner.addResult('Replacing 0, 1 with 25-36');
list.replaceItemsInRange(0, 2, [25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36]);
dumpList();

TestRunner.addResult('Scrolling to 18');
list.scrollItemAtIndexIntoView(28);
dumpList();

TestRunner.addResult('Replacing 25-36 with 0-1');
list.replaceItemsInRange(0, 12, [0, 1]);
dumpList();

TestRunner.addResult('Replacing 16-18 with 45');
list.replaceItemsInRange(16, 19, [45]);
dumpList();

TestRunner.addResult('Scrolling to 4');
list.scrollItemAtIndexIntoView(4);
dumpList();

TestRunner.addResult('Replacing 45 with 16-18');
list.replaceItemsInRange(16, 17, [16, 17, 18]);
dumpList();

TestRunner.addResult('Resizing');
list.element.style.height = '190px';
list.viewportResized();
dumpList();

TestRunner.completeTest();
