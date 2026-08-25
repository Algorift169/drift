const vscode = require("vscode");

const blockHeaderPattern = /^\s*(?:if|elif|else|repeat|for|while|each|unless|when)\b.*:\s*$/;
const caseHeaderPattern = /^\s*(?!if\b|elif\b|else\b|repeat\b|for\b|while\b|each\b|unless\b|when\b|end\b)[^:]+:\s*$/;

function indentationWidth(text, tabSize) {
  return text.replace(/\t/g, " ".repeat(tabSize)).length;
}

function indentationText(width, useSpaces, tabSize) {
  return useSpaces ? " ".repeat(width) : "\t".repeat(Math.floor(width / tabSize));
}

function matchingBlockIndent(editor, lineNumber, currentWidth, tabSize) {
  for (let index = lineNumber - 1; index >= 0; index -= 1) {
    const text = editor.document.lineAt(index).text;
    const trimmed = text.trim();
    const width = indentationWidth(text.match(/^[ \t]*/)[0], tabSize);

    if (!trimmed || trimmed === "else:" || trimmed.startsWith("elif ") || caseHeaderPattern.test(text)) {
      continue;
    }
    if (trimmed.startsWith("end")) {
      continue;
    }
    if (blockHeaderPattern.test(text) && width < currentWidth) {
      return width;
    }
  }
  return Math.max(0, currentWidth - tabSize);
}

function exitOneBlock() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== "drift") {
    return;
  }

  const tabSize = editor.options.tabSize || 4;
  const useSpaces = editor.options.insertSpaces !== false;
  const position = editor.selection.active;
  const line = editor.document.lineAt(position.line);
  const leadingWhitespace = line.text.match(/^[ \t]*/)[0];
  const indentWidth = useSpaces ? tabSize : 1;
  const currentWidth = leadingWhitespace.replace(/\t/g, " ".repeat(tabSize)).length;
  const nextWidth = Math.max(0, currentWidth - indentWidth);
  const nextIndent = useSpaces
    ? " ".repeat(nextWidth)
    : "\t".repeat(Math.floor(nextWidth / tabSize));
  const insertion = "\n" + nextIndent;

  editor.edit((editBuilder) => {
    editBuilder.insert(position, insertion);
  }).then((applied) => {
    if (applied) {
      const nextPosition = new vscode.Position(position.line + 1, nextIndent.length);
      editor.selection = new vscode.Selection(nextPosition, nextPosition);
    }
  });
}

function enter() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== "drift") {
    return;
  }

  const position = editor.selection.active;
  const line = editor.document.lineAt(position.line);
  const prefix = line.text.slice(0, position.character);
  const header = /^\s*(?:if|elif|else|repeat|for|while|each|unless|when)\b.*:\s*$/.test(prefix);
  const caseHeader = /^\s*(?!if\b|elif\b|else\b|repeat\b|for\b|while\b|each\b|unless\b|when\b|end\b)[^:]+:\s*$/.test(prefix);
  const branchHeader = /^\s*(?:else|elif\b.*|\d+|true|false|[A-Za-z_][A-Za-z0-9_-]*)\s*:\s*$/.test(prefix);
  const currentIndent = line.text.match(/^[ \t]*/)[0];
  const tabSize = editor.options.tabSize || 4;
  const useSpaces = editor.options.insertSpaces !== false;
  const indentUnit = useSpaces ? " ".repeat(tabSize) : "\t";
  let nextIndent = currentIndent;
  let lineIndent = currentIndent;

  if (/^\s*end\b/.test(prefix)) {
    const endWidth = matchingBlockIndent(editor, position.line, indentationWidth(currentIndent, tabSize), tabSize);
    lineIndent = indentationText(endWidth, useSpaces, tabSize);
    nextIndent = lineIndent;
  }

  if (branchHeader && !header && position.line > 0) {
    let previousLine = position.line - 1;
    while (previousLine >= 0 && editor.document.lineAt(previousLine).text.trim() === "") {
      previousLine--;
    }
    if (previousLine >= 0 && !/^\s*when\b.*:\s*$/.test(editor.document.lineAt(previousLine).text)) {
      lineIndent = currentIndent.length >= indentUnit.length
        ? currentIndent.slice(0, -indentUnit.length)
        : "";
    }
  }

  if (!/^\s*end\b/.test(prefix) && (header || caseHeader || branchHeader)) {
    nextIndent += indentUnit;
  }

  if (lineIndent !== currentIndent) {
    nextIndent = lineIndent + indentUnit;
  }

  editor.edit((editBuilder) => {
    if (lineIndent !== currentIndent) {
      editBuilder.delete(new vscode.Range(position.line, 0, position.line, currentIndent.length));
      editBuilder.insert(new vscode.Position(position.line, 0), lineIndent);
    }
    editBuilder.insert(position, "\n" + nextIndent);
  }).then((applied) => {
    if (applied) {
      const nextPosition = new vscode.Position(position.line + 1, nextIndent.length);
      editor.selection = new vscode.Selection(nextPosition, nextPosition);
    }
  });
}

function activate(context) {
  context.subscriptions.push(
    vscode.commands.registerCommand("drift.exitOneBlock", exitOneBlock),
    vscode.commands.registerCommand("drift.enter", enter)
  );
}

function deactivate() {}

module.exports = { activate, deactivate };