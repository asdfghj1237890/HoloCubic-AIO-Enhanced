.PHONY: dev lint format typecheck test build run

dev:
	uv sync --all-groups

lint:
	uv run ruff check .

format:
	uv run ruff format .

typecheck:
	uv run ty check .

test:
	uv run pytest

run:
	uv run python CubicAIO_Tool.py

build:
	uv run pyinstaller CubicAIO_Tool.spec
