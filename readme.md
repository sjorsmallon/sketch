# Lessons learned

Do not just bind anything randomly before binding the VAO. that causes a lot of hard-to-debug issues (`glDrawElementsInstanced` just crashed. cannot even set a breakpoint or nothing. renderDoc does not even capture anything.)

Renderdoc is truly one of the most useful tools. do not forget that you can inspect the buffers via a small right-hand side button.
First intuitions were correct. Only two buffers were showing up in renderdoc, where there should have been three. (IBO, vertices, offsets.)