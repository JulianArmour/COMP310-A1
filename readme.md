
# Notes For Grader  
This api syntax for running programs is taken straight from the assignment handout, but I thought I'd explicitly write it out just in case.  
## Supplying a FIFO to my TinyShell  
My tiny shell takes in the path to the user-created FIFO when the program is initially launched.  
  
`$ tshell .../FIFOfile`  
## Piping 2 programs  
The syntax for running the two piped programs for my tiny shell is as follows:  
- `>>> .../program1 <args1> | .../program2 <args2>`  
  
- e.g. `>>> echo "Hello world!" | wc`