# Drift VS Code Support

This folder contains a minimal VS Code language extension for the Drift scripting language.

## Included files

- `package.json` — extension manifest that registers the `drift` language and its grammar
- `language-configuration.json` — editor settings for comments, brackets, and auto-closing behavior
- `syntaxes/drift.tmLanguage.json` — TextMate grammar for syntax highlighting
- `drift-language-support-0.1.0.vsix` — packaged extension for local installation

## Supported language features

The current extension provides:

- `.df` file recognition
- basic syntax highlighting for Drift keywords, operators, strings, numbers, comments, and identifiers
- bracket matching and comment configuration via the language configuration file

## Install locally

From this directory, you can install the packaged extension with:

```bash
code --install-extension drift-language-support-0.1.0.vsix
```

Or, from the VS Code Extensions view, install the `.vsix` package manually.

## Develop it further

This is intentionally a lightweight extension. It does not yet include:

- IntelliSense or autocomplete
- semantic validation
- debugger support
- custom snippets
- linting or formatting

If you extend the language, the main files to update are:

- `package.json` for language registration and contribution points
- `language-configuration.json` for editor behavior
- `syntaxes/drift.tmLanguage.json` for highlighting rules

## Example

A typical Drift source file is just a `.df` file, for example:

```drift
var x = 42
say x

if x > 10:
    say "greater"
else:
    say "smaller"
```

This extension is intended to make Drift files easier to read and edit inside VS Code while the language itself continues to evolve.
