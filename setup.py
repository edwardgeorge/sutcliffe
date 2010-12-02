from setuptools import setup, Extension

ext = Extension("sutcliffe._sutcliffe",
    sources=["sutcliffe/_sutcliffe.c"],
    extra_link_args = [
        '-framework', 'IOKit',
        '-framework', 'CoreFoundation'],
)

setup(name="sutcliffe", version="0.1",
      ext_modules=[ext],
)
