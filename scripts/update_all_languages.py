"""
This script updates Qt translation (.ts) files by matching English source strings
tagged as 'unfinished' with translation strings stored in external JSON files.

How it works:
1. Dynamically loads all JSON files from translations/languages/ (e.g., zh_CN.json).
2. Parses each corresponding .ts file (XML) in the translations/ directory.
3. For each <message> tagged with type="unfinished":
   - Looks up the <source> text in the corresponding JSON (using context and source key).
   - If a match is found, updates the <translation> content.
   - Removes the type="unfinished" attribute.
4. Post-processes the XML to match Qt's lupdate formatting (e.g., &apos; instead of ').

Maintenance Workflow:
1. Identify missing translations: Open a .ts file and look for <translation type="unfinished">.
2. Note the <context> name and the <source> string.
3. Update JSON: Add the context and key-value pair to the relevant JSON file in
   translations/languages/XX.json.
4. Add new language: Create a new XX.json in the languages/ folder.
5. Run the script: python3 scripts/update_all_languages.py

Who maintains the JSON files?
- Developers: Update JSON when adding new features or strings.
- Community: Submit PRs to edit the simple JSON files instead of Python code.
- Tools: Can be uploaded to translation platforms (Crowdin, Weblate) which support JSON.
"""
import xml.etree.ElementTree as ET
import sys
import os
import json


def load_translations(lang_dir):
    """
    Loads all JSON translation files from the specified directory.
    Returns:
        dict: {lang_code: translations_map}
    """
    translations = {}
    if not os.path.exists(lang_dir):
        print(f"Warning: Languages directory {lang_dir} not found.")
        return translations

    for filename in os.listdir(lang_dir):
        if filename.endswith(".json"):
            lang_code = filename[:-5]
            filepath = os.path.join(lang_dir, filename)
            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    translations[lang_code] = json.load(f)
            except Exception as e:
                print(f"Error loading {filename}: {e}")

    return translations


def update_translations(ts_file, translations_map):
    """
    Updates a Qt .ts file with translations from a map.
    Args:
        ts_file (str): Path to the .ts file.
        translations_map (dict): Dict of {context: {source_text: translation_text}}.
    """
    print(f"Processing {ts_file}...")
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except Exception as e:
        print(f"Error parsing XML: {e}")
        return

    updated_count = 0

    for context in root.findall('context'):
        name_node = context.find('name')
        if name_node is None or name_node.text not in translations_map:
            continue

        context_name = name_node.text
        context_translations = translations_map[context_name]

        for message in context.findall('message'):
            source = message.find('source')
            if source is None or source.text not in context_translations:
                continue

            translation_node = message.find('translation')
            if translation_node is None:
                translation_node = ET.SubElement(message, 'translation')

            new_text = context_translations[source.text]

            # Update text
            translation_node.text = new_text

            # Remove type="unfinished" or similar attributes if they exist
            if 'type' in translation_node.attrib:
                del translation_node.attrib['type']

            updated_count += 1

    if updated_count > 0:
        # standard ElementTree write
        tree.write(ts_file, encoding='utf-8', xml_declaration=True)

        # Post-process to mimic lupdate format (e.g. escaping single quotes)
        with open(ts_file, 'r', encoding='utf-8') as f:
            content = f.read()

        # Qt's lupdate prefers &apos; for single quotes in text
        if "'" in content:
            content = content.replace("'", "&apos;")

            # Fix XML declaration which might have used single quotes originally and got escaped
            content = content.replace(
                "version=&apos;1.0&apos;", 'version="1.0"')
            content = content.replace(
                "encoding=&apos;utf-8&apos;", 'encoding="utf-8"')

            with open(ts_file, 'w', encoding='utf-8') as f:
                f.write(content)

        print(
            f"Successfully updated {updated_count} translations in {ts_file}")
    else:
        print(f"No translations matched or updated in {ts_file}.")


def main():
    base_dir = os.path.abspath(os.path.join(
        os.path.dirname(__file__), "..", "translations"))
    lang_dir = os.path.join(base_dir, "languages")

    languages = load_translations(lang_dir)

    if not languages:
        print("No translations found to process.")
        return

    for lang_code, translations in languages.items():
        filename = f"deskflow_{lang_code}.ts"
        filepath = os.path.join(base_dir, filename)

        if not os.path.exists(filepath):
            print(f"Skipping {lang_code}: {filepath} not found")
            continue

        update_translations(filepath, translations)


if __name__ == "__main__":
    main()
