# Lessons learned

Do not just bind anything randomly before binding the VAO. that causes a lot of hard-to-debug issues (`glDrawElementsInstanced` just crashed. cannot even set a breakpoint or nothing. renderDoc does not even capture anything.)

Renderdoc is truly one of the most useful tools. do not forget that you can inspect the buffers via a small right-hand side button.
First intuitions were correct. Only two buffers were showing up in renderdoc, where there should have been three. (IBO, vertices, offsets.)


The ECS approach used here is not because I think it is a very good idea, but because I have a habit of getting swamped and overwhelmed with what I'm doing. It allows me to compartmentalize some of my implementation details. If I ever want to remove e.g. jolt from this project, I can just delete the system and component file and be done with it. in the hopes that it does not leak into the rest of this code.

The physics system is kind of overwhelming. Note that the physics are executed with a fixed timestep in order to guarantee determinism. Also note how it is implemented to allow you 
to hijack the logging as well as the asserts. Kind of clever!

