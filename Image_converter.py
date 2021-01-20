'''
A quick tool for converting a black and white photo into hexadecimal format
for TI Sensortag.

@author Jimi Käyrä
'''

from PIL import Image

img = Image.open("img.png", "r") # Open the image...
pix_list = list(img.getdata()) # ...and get the pixel data.

amount = 0
string = "" # Initialize the empty string.

for pixel in pix_list: # For every pixel:
    if pixel[0] == 0: # the pixel is black
        string = string + "0"
        amount += 1
    elif pixel[0] == 255: # the pixel is white
        string = string + "1"
        amount += 1

    if amount == 8: # When one byte length has been reached...
        decimal_representation = int(string, 2) # convert to decimal
        hexadecimal_string = hex(decimal_representation) # and to hex

        print(hexadecimal_string + ", ", end = '') # Print the string
        amount = 0
        string = ""
