import os

doc = """# Test

## Hello
World
"""

with open('prediction-v3.md', 'w', encoding='utf-8') as f:
    f.write(doc)
print('OK')
