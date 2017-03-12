# BUILD-IT BREAK-IT

This is a exercise of Secure Coding course. The exercise consists of two parts:

## BUILD-IT

In this part, we are required to write C program to meet the following requirement:

To protect a legacy medical device running Windows XP that must allow operators to visit a website, you want to remove all scripts from websites. Disabling scripting in the browser is not an option, as making any changes to the configuration will require recertification by the US Food and Drug Administration (FDA). Instead you are going to create a program to run on the proxy server to strip all scripts from a webpage.

**Usage**

remove\_scripts [-i filename] [-o filename]

remove\_scripts takes in a webpage (HTML file) from stdin (or a file as specified with the –i option), and writes an equivalent webpage to stdout (or a file as specified with the –o option) will all scripts removed. All other content on the webpage must remain.

Pay close attention to the potential for scripts to appear outside of **\<script\>** tags.

Example input:

``` HTML
<!DOCTYPE html>
<html>
<body>

<h1>What Can JavaScript Do?</h1>

<p id="demo">JavaScript can change HTML content.</p>

<button type="button" onclick='document.getElementById("demo").innerHTML = "Hello JavaScript!"'>Click Me!</button>

</body>
</html>
```

Desired output (note that only the script is removed, and all other HTML elements, including the button remain):

``` HTML
<!DOCTYPE html>
<html>
<body>

<h1>What Can JavaScript Do?</h1>

<p id="demo">JavaScript can change HTML content.</p>

<button type="button">Click Me!</button>

</body>
</html>
```
