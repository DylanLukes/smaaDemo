#
# Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

import argparse
import json
from PIL import Image, ImageDraw, ImageFont


PROGRAM_VERSION = 'VMA Dump Visualization 1.0.0'
IMG_SIZE_X = 800
IMG_MARGIN = 8
FONT_SIZE = 10
MAP_SIZE = 24
COLOR_TEXT_H1 = (0, 0, 0, 255)
COLOR_TEXT_H2 = (150, 150, 150, 255)
COLOR_OUTLINE = (160, 160, 160, 255)
COLOR_OUTLINE_HARD = (0, 0, 0, 255)
COLOR_GRID_LINE = (224, 224, 224, 255)


argParser = argparse.ArgumentParser(description='Visualization of Vulkan Memory Allocator JSON dump.')
argParser.add_argument('DumpFile', type=argparse.FileType(mode='r', encoding='UTF-8'), help='Path to source JSON file with memory dump created by Vulkan Memory Allocator library')
argParser.add_argument('-v', '--version', action='version', version=PROGRAM_VERSION)
argParser.add_argument('-o', '--output', required=True, help='Path to destination image file (e.g. PNG)')
args = argParser.parse_args()

data = {}


def ProcessBlock(dstBlockList, objBlock):
    iBlockSize = int(objBlock['TotalBytes'])
    arrSuballocs  = objBlock['Suballocations']
    dstBlockObj = {'Size':iBlockSize, 'Suballocations':[]}
    dstBlockList.append(dstBlockObj)
    for objSuballoc in arrSuballocs:
        dstBlockObj['Suballocations'].append((objSuballoc['Type'], int(objSuballoc['Size'])))


def GetDataForMemoryType(iMemTypeIndex):
    global data
    if iMemTypeIndex in data:
        return data[iMemTypeIndex]
    else:
        newMemTypeData = {'DedicatedAllocations':[], 'DefaultPoolBlocks':[], 'CustomPoolBlocks':[]}
        data[iMemTypeIndex] = newMemTypeData
        return newMemTypeData


# Returns tuple:
# [0] image height : integer
# [1] pixels per byte : float
def CalcParams():
    global data
    iImgSizeY = IMG_MARGIN
    iImgSizeY += FONT_SIZE + IMG_MARGIN # Grid lines legend - sizes
    iMaxBlockSize = 0
    for dictMemType in data.values():
        iImgSizeY += IMG_MARGIN + FONT_SIZE
        iImgSizeY += len(dictMemType['DedicatedAllocations']) * (IMG_MARGIN * 2 + FONT_SIZE + MAP_SIZE)
        for tDedicatedAlloc in dictMemType['DedicatedAllocations']:
            iMaxBlockSize = max(iMaxBlockSize, tDedicatedAlloc[1])
        iImgSizeY += len(dictMemType['DefaultPoolBlocks']) * (IMG_MARGIN * 2 + FONT_SIZE + MAP_SIZE)
        for objBlock in dictMemType['DefaultPoolBlocks']:
            iMaxBlockSize = max(iMaxBlockSize, objBlock['Size'])
        iImgSizeY += len(dictMemType['CustomPoolBlocks']) * (IMG_MARGIN * 2 + FONT_SIZE + MAP_SIZE)
        for objBlock in dictMemType['CustomPoolBlocks']:
            iMaxBlockSize = max(iMaxBlockSize, objBlock['Size'])
    fPixelsPerByte = (IMG_SIZE_X - IMG_MARGIN * 2) / float(iMaxBlockSize)
    return iImgSizeY, fPixelsPerByte


def TypeToColor(sType):
    if sType == 'FREE':
        return 220, 220, 220, 255
    elif sType == 'BUFFER':
        return 255, 255, 0, 255
    elif sType == 'IMAGE_OPTIMAL':
        return 128, 255, 255, 255
    elif sType == 'IMAGE_LINEAR':
        return 64, 255, 64, 255
    assert False
    return 0, 0, 0, 255


def DrawDedicatedAllocationBlock(draw, y, tDedicatedAlloc):
    global fPixelsPerByte
    iSizeBytes = tDedicatedAlloc[1]
    iSizePixels = int(iSizeBytes * fPixelsPerByte)
    draw.rectangle([IMG_MARGIN, y, IMG_MARGIN + iSizePixels, y + MAP_SIZE], fill=TypeToColor(tDedicatedAlloc[0]), outline=COLOR_OUTLINE)


def DrawBlock(draw, y, objBlock):
    global fPixelsPerByte
    iSizeBytes = objBlock['Size']
    iSizePixels = int(iSizeBytes * fPixelsPerByte)
    draw.rectangle([IMG_MARGIN, y, IMG_MARGIN + iSizePixels, y + MAP_SIZE], fill=TypeToColor('FREE'), outline=None)
    iByte = 0
    iX = 0
    iLastHardLineX = -1
    for tSuballoc in objBlock['Suballocations']:
        sType = tSuballoc[0]
        iByteEnd = iByte + tSuballoc[1]
        iXEnd = int(iByteEnd * fPixelsPerByte)
        if sType != 'FREE':
            if iXEnd > iX + 1:
                draw.rectangle([IMG_MARGIN + iX, y, IMG_MARGIN + iXEnd, y + MAP_SIZE], fill=TypeToColor(sType), outline=COLOR_OUTLINE)
                # Hard line was been overwritten by rectangle outline: redraw it.
                if iLastHardLineX == iX:
                    draw.line([IMG_MARGIN + iX, y, IMG_MARGIN + iX, y + MAP_SIZE], fill=COLOR_OUTLINE_HARD)
            else:
                draw.line([IMG_MARGIN + iX, y, IMG_MARGIN + iX, y + MAP_SIZE], fill=COLOR_OUTLINE_HARD)
                iLastHardLineX = iX
        iByte = iByteEnd
        iX = iXEnd


def BytesToStr(iBytes):
    if iBytes < 1024:
        return "%d B" % iBytes
    iBytes /= 1024
    if iBytes < 1024:
        return "%d KiB" % iBytes
    iBytes /= 1024
    if iBytes < 1024:
        return "%d MiB" % iBytes
    iBytes /= 1024
    return "%d GiB" % iBytes


jsonSrc = json.load(args.DumpFile)
if 'DedicatedAllocations' in jsonSrc:
    for tType in jsonSrc['DedicatedAllocations'].items():
        sType = tType[0]
        assert sType[:5] == 'Type '
        iType = int(sType[5:])
        typeData = GetDataForMemoryType(iType)
        for objAlloc in tType[1]:
            typeData['DedicatedAllocations'].append((objAlloc['Type'], int(objAlloc['Size'])))
if 'DefaultPools' in jsonSrc:
    for tType in jsonSrc['DefaultPools'].items():
        sType = tType[0]
        assert sType[:5] == 'Type '
        iType = int(sType[5:])
        typeData = GetDataForMemoryType(iType)
        for objBlock in tType[1]['Blocks']:
            ProcessBlock(typeData['DefaultPoolBlocks'], objBlock)
if 'Pools' in jsonSrc:
    arrPools = jsonSrc['Pools']
    for objPool in arrPools:
        iType = int(objPool['MemoryTypeIndex'])
        typeData = GetDataForMemoryType(iType)
        arrBlocks = objPool['Blocks']
        for objBlock in arrBlocks:
            ProcessBlock(typeData['CustomPoolBlocks'], objBlock)
            

iImgSizeY, fPixelsPerByte = CalcParams()

img = Image.new('RGB', (IMG_SIZE_X, iImgSizeY), 'white')
draw = ImageDraw.Draw(img)

try:
    font = ImageFont.truetype('segoeuib.ttf')
except:
    font = ImageFont.load_default()

y = IMG_MARGIN

# Draw grid lines
iBytesBetweenGridLines = 32
while iBytesBetweenGridLines * fPixelsPerByte < 64:
    iBytesBetweenGridLines *= 2
iByte = 0
while True:
    iX = int(iByte * fPixelsPerByte)
    if iX > IMG_SIZE_X - 2 * IMG_MARGIN:
        break
    draw.line([iX + IMG_MARGIN, 0, iX + IMG_MARGIN, iImgSizeY], fill=COLOR_GRID_LINE)
    if iX + 32 < IMG_SIZE_X - 2 * IMG_MARGIN:
        draw.text((iX + IMG_MARGIN + FONT_SIZE/4, y), BytesToStr(iByte), fill=COLOR_TEXT_H2, font=font)
    iByte += iBytesBetweenGridLines
y += FONT_SIZE + IMG_MARGIN

# Draw main content
for iMemTypeIndex in sorted(data.keys()):
    dictMemType = data[iMemTypeIndex]
    draw.text((IMG_MARGIN, y), "Memory type %d" % iMemTypeIndex, fill=COLOR_TEXT_H1, font=font)
    y += FONT_SIZE + IMG_MARGIN
    index = 0
    for tDedicatedAlloc in dictMemType['DedicatedAllocations']:
        draw.text((IMG_MARGIN, y), "Dedicated allocation %d" % index, fill=COLOR_TEXT_H2, font=font)
        y += FONT_SIZE + IMG_MARGIN
        DrawDedicatedAllocationBlock(draw, y, tDedicatedAlloc)
        y += MAP_SIZE + IMG_MARGIN
        index += 1
    index = 0
    for objBlock in dictMemType['DefaultPoolBlocks']:
        draw.text((IMG_MARGIN, y), "Default pool block %d" % index, fill=COLOR_TEXT_H2, font=font)
        y += FONT_SIZE + IMG_MARGIN
        DrawBlock(draw, y, objBlock)
        y += MAP_SIZE + IMG_MARGIN
        index += 1
    index = 0
    for objBlock in dictMemType['CustomPoolBlocks']:
        draw.text((IMG_MARGIN, y), "Custom pool block %d" % index, fill=COLOR_TEXT_H2, font=font)
        y += FONT_SIZE + IMG_MARGIN
        DrawBlock(draw, y, objBlock)
        y += MAP_SIZE + IMG_MARGIN
        index += 1
del draw
img.save(args.output)

"""
Main data structure - variable `data` - is a dictionary. Key is integer - memory type index. Value is dictionary of:
- Fixed key 'DedicatedAllocations'. Value is list of tuples, each containing:
    - [0]: Type : string
    - [1]: Size : integer
- Fixed key 'DefaultPoolBlocks'. Value is list of objects, each containing dictionary with:
    - Fixed key 'Size'. Value is int.
    - Fixed key 'Suballocations'. Value is list of tuples as above.
- Fixed key 'CustomPoolBlocks'. Value is list of objects, each containing dictionary with:
    - Fixed key 'Size'. Value is int.
    - Fixed key 'Suballocations'. Value is list of tuples as above.
"""
