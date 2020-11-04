# About Forque

The goal of "forque" structure in this library is to allow items to be scheduled for processing based on hieratical tags attached to them. Forque ensures that at most one item is processed at a time for all tags related to the item’s tag, while respecting reserved order of related items.

Structure of tag can be symmetric or static. Static tags have depth and types of each level are defined at compile-time. For dynamic tags depth is not defined, and types are defined in run-time and can be different even for the children of the same tag.

| Level: | 1 | 2 | 3 |
| --- | --- | --- | --- |
| Type: | `int` | `string` | `float` |
| Static Tag #1 | `77` | `"x"` | `12.3` |
| Static Tag #2 | `8` | `"y"` |  |
| Static Tag #3 | `10` |  |  |


| Level: | 1 | 2 | 3 |
| --- | --- | --- | --- |
| Dynamic Tag #1 | `8` | `"x"` | `12.3` |
| Dynamic Tag #2 | `8` | `7.9` |  |
| Dynamic Tag #3 | `"y"` | `8` |  |


Producing item is done in two phases:
1. Reserving place in the queue
2. Populating item with actual playload

Reserving place ensures that relative order of related items will be respected, so that time needed to populate item does not cause unwanted reordering.

Only populated items will be served to the consumers. All related items ordered after the item will wait for it to be populated, served and consumed before they are offered to the consumer, even if they are populated.

These two phases can be merged and item can be populated at reservation time, but the order will still be respected and the new item will not be offered unless there are no other related items before it.

Like producing, consuming is also done in two phases:
1. Obtaining populated item
2. Releasing item from the queue

The two phase process ensures that consumers will not be served with related items concurrently. In the first phase consumer asks for an available time. After the item is processed, consumer has to notify queue that processing is done, so the next available item with related tag, if any, can be offered.

While the order of related items is respected, no such guarantee exists for unrelated items and that is the whole point of the exercise :) There is still some control over the order in which available, but unrelated items are served the consumers:
1.	FIFO - the first item made available gets served first
2.	LIFO - the last item made available gets served first
3.	PRORITY - available item with highest user-defined priority  gets served first


