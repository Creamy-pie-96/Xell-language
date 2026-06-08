**Found bug in the @safe_loop. it naievely checks for break inside the loop and if it find one just accepts it leading to this bug:

```xell
@safe_loop
loop : 
    print("yo")
    for i in [1,3]:
        print(i)
        break
    ;
;
```
it will be fooled here cz even if it has a break statement it's not of 'loop' loop but 'for' loop.