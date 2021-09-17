# Systems-Programming-and-Concurrency
Lab Projects Developed in ECE252

This projects implemented concepts learned in ECE252. Relevant concepts include: Multi-threads, Shared memory between threads, Shared memory between processes, and Asynchronous I/O. A summary of each lab project is decribed below, and detailed objectives and lab requirements are included in ece252_manual.pdf.

### Lab1: Introduction to systems programming in Linux computing environment
This lab is to introduce system programming in a general Linux Development Environment. There are three small projects in this lab which achieve the functionalities of: 
	* pnginfo: output the size of a png image and display error if the input file is not a png or has wrong CRC value
	* findpng: search for png files in a directory hierarchy
	* catpng: concatenate png images vertically to a new PNG named all.png


### Lab2: Multi-threaded concurrency programming with blocking I/O
This lab is a multi-thread implementation to request a resource across the network using blocking I/O, and the objective of it is to request all horizontal strips with random order of a picture from the server and then concatenate these strips to restore the original picture.


### Lab3: Inter-process communication and concurrency control
This lab is a project in interprocess communication and concurrency control using shared memory between processes. The objective is the same as lab2, but this time a producer and consumer structure is used. The producers will make requests to the lab web server and together they will fetch all 50 distinct image segments, and each consumer reads image segments out of the buffer, one at a time, then the consumer will process the received data.


### Lab4: Parallel web crawling
This lab is to design and implement a multi-threaded web crawler using shared memory between threads. The object of it to search the web given a seed URL and find all the URLs that link to png images, and then restore the original image using methods in previous labs.

### Lab5: Single-threaded concurrency programming with asynchronous I/O
This lab is to design and implement a single-threaded web crawler using non-block I/O. The objective is the same as Lab4, but this time asynchronous I/O is used.
