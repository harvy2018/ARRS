rd /Q /S project\Objects
rd /Q /S project\Listings
del /Q project\gasolineTransactionSampling.uvguix.*
attrib "project\.svn\text-base\*.*" /s /d -r -s
del /Q project\.svn\text-base\*.svn-base  
del /Q project\JLinkLog.txt
del /Q output\*.bin 
del /Q output\*.hex



