# Keyboard controller

## Key state handling

* 0: 1st trigger
* 1: 2nd trigger
* 2: direction
    * 0: down
    * 1: up


### States
| 1st trigger | 2nd trigger | direction | state | action |
|:-: | :-: | :-: | :--: | :---: |
| 0 | 0 | 0 | key is up | --- |
| 1 | 0 | 0 | key going down | save current time |
| 1 | 1 | 0 | key is down | compare saved time with current time and save difference |
| 1 | 1 | 1 | key is down | --- |
| 1 | 0 | 1 | key is going up | save current time |
| 0 | 0 | 1 | key is up | compare saved time with current time and save difference |


## Performance-Optimization

* Kein Serial
* Pins direkt schalten
