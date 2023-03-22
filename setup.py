# -*- coding: utf-8 -*-
import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setuptools.setup(
    name="otbtf",
    version="3.4.0",
    author="Remi Cresson",
    author_email="remi.cresson@inrae.fr",
    description="OTBTF: Orfeo ToolBox meets TensorFlow",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://gitlab.irstea.fr/remi.cresson/otbtf",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Topic :: Scientific/Engineering :: GIS",
        "Topic :: Scientific/Engineering :: Image Processing",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
    ],
    packages=setuptools.find_packages(),
    python_requires=">=3.6",
    keywords="remote sensing, otb, orfeotoolbox, orfeo toolbox, tensorflow, tf, deep learning, machine learning",
)
