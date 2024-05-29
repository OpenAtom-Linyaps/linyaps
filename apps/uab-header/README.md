# uab-header

**uab-header** is a template binary which is used by ll-builder to build uab *(Universal Application Bundle)*.

Due to one of the goals of Uab is to depends as few runtime dependencies as possible, uab-header will **statically link all dependent libraries** at compile time. It is important to note that when some of the system's base libraries are upgraded, it may cause uab to fail to run properly

It's not an executable binary in the traditional sense, so we **shouldn't** add executable permissions to it when installing it to the system.
