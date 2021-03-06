# Forque Library

The goal of "forque" structure in this library is to allow items to be scheduled for processing based on hierarchical tags attached to them. Forque ensures that at most one item is processed at a given time for all tags related to the item’s own tag, while respecting relative order among related items in which they have been reserved in the queue. Tags are said to be related if one of them are descendant of the other. For example tags `1/2` and `1/2/3` are related, while tags `1/2/1` and `1/2/3` are not related. 

![forque structure](https://user-images.githubusercontent.com/42535720/98058027-0c6cd300-1e44-11eb-8303-5a5ebdc70bea.png)

Structure of tag can be dynamic or static. Static tags have depth and types of each level defined at compile-time. For dynamic tags, depth is not defined while types can change during run-time and they can be of different type event for sibling tags.

| Level: | 0 | 1 | 2 |
| --- | --- | --- | --- |
| Type: | `int` | `string` | `float` |
| Static Tag #1 | `77` | `"x"` | `12.3` |
| Static Tag #2 | `8` | `"y"` |  |
| Static Tag #3 | `10` |  |  |


| Level: | 0 | 1 | 1 |
| --- | --- | --- | --- |
| Dynamic Tag #1 | `8` | `"x"` | `12.3` |
| Dynamic Tag #2 | `8` | `7.9` |  |
| Dynamic Tag #3 | `"y"` | `8` |  |


Production of an item is done in two phases:
1. Reserving place in the queue
2. Populating item with actual payload

Reserving place ensures that relative order of related items will be respected, so that time needed to populate item with actual value does not cause unwanted reorderings.

Only populated items will be served to the consumers. All related items ordered after the item will wait for it to be populated, served and consumed before they are offered to the consumer, even if they are populated.

These two phases can be merged and item can be populated at reservation time, but the order will still be respected and the new item will not be offered unless there are no other related items that have to be consumed before it.

Like production, consumption is also done in two phases:
1. Obtaining populated item
2. Releasing item from the queue

The two phase process ensures that consumers will not be served with related items concurrently. In the first phase consumer asks for an available time. After the item is processed, consumer has to notify queue that processing is done, so the next available item with related tag, if any, can be offered.

While the order of related items is respected, no such guarantee exists for unrelated items, which is the whole point of the exercise :) There is still some control over the order in which available, but unrelated items, are served the consumers:
1. *FIFO* - the first item made available gets served first
2. *LIFO* - the last item made available gets served first
3. *Priority* - available item with highest user-defined priority  gets served first

## Requirements

Forque library is C++20 library that requires support for coroutines and concepts.


## Documentation

Documentation is available in [the wiki](https://github.com/kataklinger/forque/wiki)

## Using The Library

Source code of example application using forque is located [here](https://github.com/kataklinger/forque/tree/master/src/app).
