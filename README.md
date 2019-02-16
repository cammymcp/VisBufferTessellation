# Vulkan | Visibility Buffer with Tessellation

The visibility buffer rendering technique is a variant on deferred shading which minimises memory traffic by storing only necessary
primitive information to the g-buffer. 

Whereas a typical deferred g-buffer may be 24-32 bytes, the visibility buffer consists only of primitive ID and draw call ID packed into
a 4-byte per-sample render target in the forward pass. The deferred pass may then access geometry buffers to manually interpolate surface attributes and perform shading as normal. 

This project research implements a typical visibility buffer pipeline alongside a second which implements hardware tessellation.
In doing so, more information must be stored to the visibility buffer, and hence more bandwidth usage, but the net performance gain 
by generating detail by tessellation may prove beneficial. 

Further resources surrounding the visibility buffer and it's implementation can be found via the following resources:

Burns and Hunt (2013): http://jcgt.org/published/0002/02/04/

Schied and Daschbacher (2015): http://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf

Engel (2016, 2018): https://diaryofagraphicsprogrammer.blogspot.com/2018/03/triangle-visibility-buffer.html
