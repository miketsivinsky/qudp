del Makefile.*

del src\Makefile.*
rd src\debug /S /Q 
rd src\release /S /Q
rd src\moc /S /Q
rd src\x64 /S /Q

del test\test_rx\Makefile.*
rd test\test_rx\debug /S /Q 
rd test\test_rx\release /S /Q
rd test\test_rx\moc /S /Q
rd test\test_rx\x64 /S /Q

del test\test_tx\Makefile.*
rd test\test_tx\debug /S /Q 
rd test\test_tx\release /S /Q
rd test\test_tx\moc /S /Q
rd test\test_tx\x64 /S /Q
